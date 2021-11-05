# An example for the ns3-ai model to illustrate the data exchange
# between python-based AI frameworks and ns-3.
#
# In this example, we have two variable a and b in ns-3,
# and then put them into the shared memory using python to calculate
#
#       c = a + b
#
# Finally, we put back c to the ns-3.

import random
from ctypes import *

from py_interface import *


# The environment (in this example, contain 'a' and 'b')
# shared between ns-3 and python with the same shared memory
# using the ns3-ai model.
class Env(Structure):
    _pack_ = 1
    _fields_ = [
        ('datarate', c_double),
        ('latency', c_double)
    ]

# The result (in this example, contain 'c') calculated by python
# and put back to ns-3 with the shared memory.
class Act(Structure):
    _pack_ = 1
    _fields_ = [
        ('c', c_double)
    ]

class Array(Structure):
    _pack_ = 1
    _fields_ = [
        ('env', Env*2)
    ]


ns3Settings = {'spokes': 3, 'rounds': 3}
mempool_key = 1234                                          # memory pool key, arbitrary integer large than 1000
mem_size = 4096                                             # memory pool size in bytes
memblock_key = 2343                                        # memory block key, need to keep the same in the ns-3 script
exp = Experiment(mempool_key, mem_size, 'simple_fed_learning', '../../')      # Set up the ns-3 environment
try:
    exp.reset()                                             # Reset the environment
    rl = Ns3AIRL(memblock_key, Array, Act)                    # Link the shared memory block with ns-3 script
    ns3Settings['spokes'] = 2
    ns3Settings['rounds'] = 3
    pro = exp.run(setting=ns3Settings, show_output=True)    # Set and run the ns-3 script (sim.cc)
    while not rl.isFinish():
        with rl as data:
            if data == None:
                break
            # AI algorithms here and put the data back to the action
            print("a " + str(data.env.env[0].datarate))
            print("b " + str(data.env.env[1].datarate))
            data.act.c = 0
            
    pro.wait()                                              # Wait the ns-3 to stop
except Exception as e:
    print('Something wrong')
    print(e)
finally:
    del exp
