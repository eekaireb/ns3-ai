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

// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1
//    point-to-point
//
 

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");


struct Env{
  double datarate;
  double latency;
}Packed;

struct Act
{
    double c;
}Packed;

class FL : public Ns3AIRL<Env, Act>
{
public:
    FL(uint16_t id);
    int Func(double a, double b);
};

FL::FL(uint16_t id) : Ns3AIRL<Env, Act>(id) {
    SetCond(2, 0);      ///< Set the operation lock (even for ns-3 and odd for python).
}

int FL::Func(double a, double b)
{
    auto env = EnvSetterCond();     ///< Acquire the Env memory for writing
    env->datarate = a;
    env->latency = b;
    SetCompleted();                 ///< Release the memory and update conters
    NS_LOG_DEBUG ("Ver:" << (int)SharedMemoryPool::Get()->GetMemoryVersion(m_id));
    auto act = ActionGetterCond();  ///< Acquire the Act memory for reading
    int ret = act->c;
    GetCompleted();                 ///< Release the memory, roll back memory version and update conters
    NS_LOG_DEBUG ("Ver:" << (int)SharedMemoryPool::Get()->GetMemoryVersion(m_id));
    return ret;
}

void
ModifyLinkRate(NetDeviceContainer *ptp, DataRate lr) {
    NS_LOG_UNCOND ("Change data rate");
    StaticCast<PointToPointNetDevice>(ptp->Get(0))->SetDataRate(lr);
}

void FlowFunction(Ptr<FlowMonitor> monitor, Ptr<Ipv4FlowClassifier> classifier, FL* fl)
{
    monitor->CheckForLostPackets ();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    double a = 0;
    double b = 0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

        std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 9 / 1000 / 1000  << " Mbps\n";
        a = i->second.rxBytes * 8.0 / 9 / 1000 / 1000; //Mbps
        b = (i->second.timeLastRxPacket - i->second.timeLastTxPacket).GetDouble()/1000000000; //seconds
    }

  fl->Func(a,b);
}



int
main (int argc, char *argv[])
{

  int memblock_key = 2338;        ///< memory block key, need to keep the same in the python script
  FL fl(memblock_key);
  

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

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
    Simulator::Schedule(Seconds(i+2.5),FlowFunction, monitor, classifier, &fl);
  }
  //Simulator::Schedule(Seconds(3),FlowFunction, monitor, classifier, &fl);
  //Simulator::Schedule(Seconds(5),FlowFunction, monitor, classifier, &fl);
  //Simulator::Schedule(Seconds(4), &FL::Func, &fl, a,b);
  Simulator::Stop (Seconds (10));
  Simulator::Run();

/*
    monitor->CheckForLostPackets ();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

        std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 9 / 1000 / 1000  << " Mbps\n";
}*/
  
  Simulator::Destroy ();
  return 0; 
}
