/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ns3-ai-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

// Network Topology
//
// n1     n2     n3
//  \      |     /
//   \     |    /
//    \    |   /     
//      n4n5n6
// Trying to simulute star topology with 
// p2p links, but its a little hacky right now

 

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");


//Structure to represent the client's data
//Set by ns3
struct Env{
  double datarate;
  double latency;
}Packed;

//Some result from training
//Not used right now
struct Act
{
    double c;
}Packed;

//An array to manage client's data easily
struct Array{
  Env arr[2];
}Packed;

class FL : public Ns3AIRL<Array, Act>
{
public:
    FL(uint16_t id);
    void Func(double a, double b, int k);
};

FL::FL(uint16_t id) : Ns3AIRL<Array, Act>(id) {
    SetCond(2, 0);      ///< Set the operation lock (even for ns-3 and odd for python).
}

//access shared memory and update latency
void FL::Func(double a, double b, int k)
{
    auto env = EnvSetterCond();     ///< Acquire the Env memory for writing
    env->arr[k-1].latency = b;
    SetCompleted();                 ///< Release the memory and update conters
    NS_LOG_DEBUG ("Ver:" << (int)SharedMemoryPool::Get()->GetMemoryVersion(m_id));
}

/*
void
ModifyLinkRate(NetDeviceContainer *ptp, DataRate lr) {
    NS_LOG_UNCOND ("Change data rate");
    StaticCast<PointToPointNetDevice>(ptp->Get(0))->SetDataRate(lr);
}*/

//print out the flow results from a communication round and update the latency in ns3
//not sure this approach will work for async
void FlowFunction(Ptr<FlowMonitor> monitor, Ptr<Ipv4FlowClassifier> classifier, FL* fl, int spokes)
{
    NS_LOG_UNCOND("Begin round");
    monitor->CheckForLostPackets ();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    double a = 0;
    double b = 0;
    int k = 0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    { 
        if(k >= spokes) // since half the nodes represent the server, we only check the ones that represent clients
          break;
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

        std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 9 / 1000 / 1000  << " Mbps\n";
        a = i->second.rxBytes * 8.0 / 9 / 1000 / 1000; //Mbps
        b = (i->second.timeLastRxPacket - i->second.timeLastTxPacket).GetDouble()/1000000000; //seconds
        k++;
        fl->Func(a,b,k);
    }
    NS_LOG_UNCOND("End round");

}

int
main (int argc, char *argv[])
{
  int memblock_key = 2343;        ///< memory block key, need to keep the same in the python script, changes each time shared memory size changes
  FL fl(memblock_key);
  

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);


  //parse command line args to set spoke number in star, and number of communication rounds
  NodeContainer nodes;
  int spokes = 2; 
  CommandLine cmd;
  int rounds = 2;
  cmd.AddValue("spokes", "Number of clients", spokes);
  cmd.AddValue("rounds", "Number of rounds", rounds);
  cmd.Parse (argc, argv);
  nodes.Create (spokes*2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;

  //add p2p links between each node and then a "server"
  for(int i = 0; i <spokes; i++)
  {
    devices.Add(pointToPoint.Install(nodes.Get(i), nodes.Get(spokes+i)));
  }
  
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  int k = 1; 
  //when theres 3 spokes 
  // 0, 2, 4 are clients
  // 1, 3, 5 are servers
  for(int i = 0; i < spokes; i++)
  {
    //NS_LOG_UNCOND(interfaces.GetAddress(i));
    UdpEchoServerHelper echoServer (9);
    ApplicationContainer serverApps = echoServer.Install (nodes.Get (spokes+i));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));
    
    UdpEchoClientHelper echoClient (interfaces.GetAddress(k), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (rounds));
    //echoClient.SetAttribute ("Interval", TimeValue (Seconds (2.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps = echoClient.Install (nodes.Get (i));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));
    k+=2; 

  }

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());

  for(int i = 0; i < rounds; i++)
  {
    Simulator::Schedule(Seconds(i+2.5),FlowFunction, monitor, classifier, &fl, spokes);
  }

  Simulator::Stop (Seconds (10));
  Simulator::Run();
  
  Simulator::Destroy ();
  return 0; 
}
