
# Making hybrid systems

In every well-managed software project sooner or later we face cross-system boundaries testing needs, where we basically observe how our software cooperates with the other actors. Those actors include users as well as the other software products. Following article presents an idea of a hybrid system of asynchronous actors running in a heterogeneous environment.

## Context

To be able to verify and validate our product automatically and programmatically we should think of an easy-to-use while bound to the integration tester knowledge and work environment tooling. Usually, forcing test engineers to write tests in the language of the target solution results in a disappointing outcome in general. Dark arcana of C++ or other complex enough language used to build high-quality software is definitely opposite to every tester requires as she is focused on functionality not on the language itself. That is, language of choice for automated testing shall be easy-to learn, and to use. Such language shall come with a consistent, well-defined environment and high availability of libraries to compose with.

Let's pick Python language as our language of choice for automated testing, and C++ as a language of software products which cooperation we want to observe and to alter. We will produce test scripts that emulate both parties of the communication depending on a test context.

Our system under test consists of multiple distributed multithreaded applications (processes, nodes, etc.), where each one consumes and produces information asynchronously. Applications expose well-defined interfaces with finite sets of communication patterns. Expected information flow encoded as scenarios is known but is irrelevant to us: we shall be able to build any scenario inside a test script.

To be concrete, we may anchor ourselves in a contemporary automotive environment, where parties are either [AUTOSAR Adaptive](https://www.autosar.org/standards/adaptive-platform/) applications (interfaces defined in ARXML, and exposed via `ara::com`), GENIVI units (interfaces defined in [Franca IDL](https://github.com/franca/franca), and exposed via GENIVI CommonAPI), or ROS nodes. Even plain REST (like in [ExVe, ISO 20078-1](https://www.iso.org/standard/66978.html)), or [MQTT](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html)-based interfaces (like in telematic control units) may be sufficient.

Applications in a heterogeneous system are able to communicate as long as we are able to provide mapping between their  ecosystems exposed via interfaces (this observation may be regarded as the essence of a hexagonal architecture). If we apply this knowledge to a homogeneous system, e.g. we have _n_ applications bound through Franca IDL/CommonAPI solely, we notice that in order to communicate Python-based ecosystem with CommonAPI-based one we need to either use (bidirectionally) foreign function interface, or introduce a thin abstraction which is well-known to both of them. While the former does not scale, the latter looks promising.

## From non-determinism to a deterministic behaviour

Raw (non-abstracted out) execution of asynchronous actions _without explicit exclusive locks_, especially if the medium is the network, leads to a non-deterministic behaviour of a system. That is, with some probability we know the _functional_ order of execution, and there _is_ plenty of _valid_ ways to get entirely wrong behaviour. As it goes even further, we know, with some degree of confidence, the architecture we eventually defined ourselves.

In ecosystems that are lacking abstraction over asynchronous execution we typically serialise the consumed input, and produce the output with help of internal state machines. Hopefully, C++ offers `co_await`, Python `await`, and platforms like AUTOSAR Adaptive either custom `Future::then` implementation, or reactor-based approaches to serialisation (see [arXiv:1912.01367v2](https://arxiv.org/pdf/1912.01367.pdf)). 
In our design we will use Python's infrastructure from `asyncio` library, and a thin reactor-like layer  based on a message queue. Since we are going to provide a reliable mapping between ecosystems, we shall assure that abstractions exposed to Python provide proper observable behaviour. To do that, we need to look closer at the communication patterns realised.

In `constr_6807` defined in [Specification of Abstract Platform for AUTOSAR Adaptive](https://www.autosar.org/fileadmin/user_upload/standards/adaptive/19-11/AUTOSAR_TPS_AbstractPlatformSpecification.pdf) `CompositeInterface`abstraction (which is a concrete instance of a `PortInterface` abstraction used to denote information that software component either produces, `PPort`, or consumes, `RPort`) two possible communication patterns are listed: `command` ("method") and `indication` ("event"). Those two have direct counterparts in [Franca IDL](https://drive.google.com/drive/folders/0B7JseVbR6jvhUnhLOUM5ZGxOOG8) (with CommonAPI binding): `method` (with optional reply as in fire-and-forget style) and `broadcast` (with optional selectivity over recipients). (Note, we intentionally omit `attribute` as it is a combination of those two mentioned.) If we select ZMQ as a thin abstraction between the ecosystems we will achieve almost perfect fit in terms of communication patterns (cf. `MDP`/`REQREP` and `PUBSUB` from [ZMQ RFC](https://rfc.zeromq.org/)).
 
## Hybrid system

As an example, we will build a Python-based solution that may simultaneously act as a server and as a client to the system under test. System under test is an asynchronous application with the interface defined in Franca IDL, and with the deployment for CommonAPI (with SOME/IP) backend.  

```

                             Hybrid system overview


              I  B   L            medium             L' B' 
+-----------+---+--+--+           +,,,,,+           +--+--+         +--------+
|           |   |  |  |====ch1===>!     !====ch1===>|  |  |         |        |
|           | F |  |  |<===ch2===>!     !<===ch2===>|  |  |         | async. |
|   async.  | I |  |  |    ...    ! ZMQ !    ...    |  |  |         | system |
|   system  | D |  |  |<===ch3===>!     !<===ch3===>|  |  |<-import-| test   |
|   (SUT)   | L |  |  |<===ch4====!     !<===ch4====|  |  |         | script |
|           |   |  |  |    ...    !     !    ...    |  |  |         |        |
|           |   |  |  |    ...    !     !    ...    |  |  |         |        |
+-----------+---+--+--+           +,,,,,+           +--+--+         +--------+
         C++      |                                      |     Python              
                  |                                      |
                  V         +-----------------+          V
                   \        |                 |         /
                    \       |  Configuration  |        / 
                     \_read_|  maps channels  |_read_ /
                            |  to ZMQ sockets |
                            |  to FIDL ifaces |
                            |                 |
                            +--<<immutable>>--+


  where
    I:   interface definition in Franca IDL (FIDL and FDEPL files),
    B:   minimal stateless logic to transfer "pickled" data from CommonAPI to ZMQ,
    L:   cppzmq,
    L':  PyZMQ,
    B':  glue code in Python: Awaitable, AsyncIterator, AsyncGenerator, etc.,
    ch1: publish-subscribe pattern, where `async system` is a publisher,
    ch2: request-reply pattern, where `async system` replies,
    ch3: publish-subscribe pattern, when `async system` is a subscriber,
    ch4: request-reply pattern, where `async system` requests,
    SUT: system under test.
```

Message queue (here: ZMQ) acts as a mean to save the state of asynchronous world, and to restore it at the other end of the communication path. 

We have to provide a static _bidirectional_ mapping from SUT interfaces (here: Franca/CommonAPI) into communication channels (here: ZMQ sockets) in a form of configuration file known to all parties. Mappings shall map data as well as actions, so that there is neither functionality nor information loss due to introduced transformation.

## User experience

Generally, all the connection setup and error handling, data marshalling ("pickling"), and required post-verification internal checks shall be transparent to the user. User shall get only relevant information, depending on the context.

We will use [Boost.Python](https://www.boost.org/doc/libs/1_72_0/libs/python/doc/html/reference/topics/pickle_support.html) (to [pickle](https://docs.python.org/3/library/pickle.html) and to define additional data properties like e.g. ["rich comparison" methods](https://docs.python.org/3/reference/datamodel.html#object.__eq__)), [cppzmq](https://github.com/zeromq/cppzmq) and [PyZMQ](https://pyzmq.readthedocs.io/en/latest/) (to communicate in a non-blocking mode). At the Python library level we will use custom `Awaitable` and `AsyncIterable` from [`collections.abc`](https://docs.python.org/3/library/collections.abc.html) objects to expose composable asynchronous end-user interface wired into the Python's world of [asyncio](https://docs.python.org/3/library/asyncio.html).

In the following example session user script receives broadcasts and issues a reply. (Note, pseudocode is used).

```python
from typing import Sequence, Tuple
from sut import verify, ScoreBoard  # warn about unhandled calls (check ZMQ queues)

from sut import Vehicle

# React once on request to handle "drive" method. 
#   Expected = Tuple[Vehicle.Drive.Input, Vehicle.Drive.Error]
#
async def testDrive(uut: Vehicle.Drive, reply: Vehicle.Drive.Output, expected: Expected) -> None:
  assert len(expected) == 2
  
  req = await uut        # Drive.__await__
  assert expected[0] == req
  
  err = await uut(reply)  # Drive.__call__ 
  assert expected[1] == err

# Reception of onRoad broadcast.
async def testOnRoad(uut: Vehicle.OnRoad, expected: Sequence[Vehicle.EventData]) -> None:
  async for msg in uut:  # OnRoad.__anext__
    assert current(expected)
    assert current(expected) == msg.event  # Vehicle.EventData.__eq__
    next(expected)

async def testSuite(fixture: Any)
  await asyncio.gather( # run in parallel
      testDrive(fixture.uut1, fixture.exp1)
    , testOnRoad(fixture.uut2, fixture.arg2, fixture.exp2)
    )

# ---

cfg: FilePath = "default.ini"
board : ScoreBoard[Any] = ScoreBoard[Vehicle]()
uut1 = Vehicle.OnRoad(cfg, board)
uut2 = Vehicle.Drive(cfg, board)
# ...
# combine data into a "fixture"

asyncio.run(testSuite(fixture))

assert board.verify(uut1, uut2)
```

Corresponding FIDL file:

```franca
interface Vehicle {
  method drive {
    in {
      GNSSCoordinates coords
    }
    out {
      GNSSCoordinates dest
      ETA             seconds
      VehicleState    state
    }
    error {
      DESTINATION_NOT_FOUND
      NEEDS_MORE_INFORMATION
      UNKNOWN
    }
  }

  broadcast onRoad {
    out {
      GNSSCoordinates coords
      EventData       event
    }
  }
}
```

What's left here is the handling of a scoreboard to check whether the expected communication really happens. This may be done by extending the [`AsyncIterator`](https://docs.python.org/3/library/collections.abc.html#collections.abc.AsyncIterator)  `Vehicle.OnRoad` and [`Awaitable`s](https://docs.python.org/3/library/collections.abc.html#collections.abc.Awaitable) produced by `Vehicle.Drive` with additional "test context" parameter in enclosing class constructor, and with logic inside the returned awaitable. Reuse of existing test frameworks written in Python is highly desirable.


#### About this document

March 28, 2020 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)

