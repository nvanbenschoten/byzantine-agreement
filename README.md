# Byzantine Agreement

_Nathan VanBenschoten_

An implementation of the Byzantine Agreement Algorithm.


## Building

Run `make` to build the binary `bin/general`

Run `make clean` to clean all build artifacts


## Running

### Commander

To run a commander process, a command like the following can be used. Note that
if the **-o** (**--order**) flag is used, the **-C** (**--commander_id**) flag
must also point to the current hostname in the hostfile.

```
./bin/general -p 54321 -h hostfile -f 1 -C 0 -o attack
```

### Lieutenant

To run a lieutenant process, a command like the following can be used.

```
./bin/general -p 54321 -h hostfile -f 1 -C 0
```

### Malicious Behavior

There are four different malicious modes that Generals can exhibit, which can be
combined with each other. The modes are:

- **silent**: the general sends no messages (lieutenants only)
- **delay_send**: the general delays the sending of each message by some random
  amount
- **partial_send**: the general occasionally drops messages instead of
  forwarding them (lieutenants only)
- **wrong_order**: the general occasionally sends the wrong order in some of its
  messages (commander only)

These malicious behavior modes can be configured using the **-m**
(**--malicious**) flag, which can be provided multiple times. An example of
running a malicious lieutenant that delays its messages and occasionally does
not send them at all is:

```
./bin/general -p 54321 -h hostfile -f 1 -C 0 -m delay_send -m partial_send
```

### Verbose Mode

Adding the **-v** (**--verbose**) flag will turn on verbose mode, which will
print logging information to standard error. This information includes details
about all messages sent and received, as well as round timeout information.

### Command Line Arguments

A full list of command line arguments can be seen by running `./bin/general --help`.


## System Architecture

The system is designed around a class hierarchy that looks like:

```
   General (abstract)
        /      \
       /        \
Commander      Lieutenant
```

When a process starts up, it processes all command line arguments and performs
validation on all flags. The validation includes checking that orders are
correctly provided for commander processes and determining the list of hosts
participating in the algorithm.

Once command line parsing and validation is complete, the options are used to
construct either a `Commander` or a `Lieutenant` instance. These class both
implement a `Decide()` method, which is called to begin the algorithm and return
the final result. Once this result is known, the process prints the result and
exists.

### General

`General` is an abstract class extended by `Commander` and `Lieutenant` that
provides mutually useful functionality. This includes the creation of UDP
Clients for all remote servers and the maintenance of the round counter.

### Commander

The `Commander` is simple. In addition to the functionality provided by
`General`, it holds the initial `Order`. During its execution of the algorithm,
it simply forwards this decision to all other processes before returning that
decision.

### Lieutenant

The `Lieutenant` is more complex that the `Commander` because it must maintain
state across multiple rounds. It does this by maintaining a set of unique
`Order`s seen, as well as a number of per-round variables. These per-round
variables determine how the `Lieutenant` acts during the duration of a round and
how the `Lieutenant` should transition to the next round and are reinitialized
at the beginning of each new round. The class run a private `udp::Server`
through which it receives messages from other `General`s and acts accordingly.
It also maintains state on timeouts to guarantee eventual termination of the
algorithm (see below for more on timeouts).

### UDP Client and Server

The abstraction of reliable communication is provided by the `udp` namespace.
This namespace exposes three classes to make dealing with UDP straightforward
for the General implementations. These classes also perform the task of hiding
away C Socket programming details behind a more idiomatic C++ interface.

First, the namespace exposes a `Client` class. The class wraps a UDP socket and
allows both unreliable and reliable (unacknowledged and acknowledged)
transmission of byte buffers. When sending reliable messages, the class allows
its caller to determine whether an acknowledgment is valid or not. The `Client`
is constructed with a remote address and an optional acknowledgment timeout.

The namespace also exposes a `Server` class. This class wraps a UDP socket and
synchronously blocks on the socket while trying to receive information. When a
new message comes in, the `Server` calls a provided callback with the messages
data as well as with a `Client` instance for consumers to respond to the remote
client who sent the message. The `Server` also handles serving timeouts, calling
a secondary timeout callback in those cases. The `Server` class is constructed
with a port to bind to and an optional timeout.

### Logging Module

The `logging` namespace provides a conditional output logger `out` that is only
enabled when verbose mode is turned on. It exposes itself as an `std::ostream`,
and forwards all information to standard error when it is enabled.


## State Diagrams

The two main components of Byzantine Agreement Algorithm are the Commander process
and the Lieutenant process. Below are illustrated state machines of their protocol,
which both end with them deciding on an Order to follow.

### Commander State Machine

![Commander State Machine](./media/commander_state_machine.png)

### Lieutenant State Machine

![Lieutenant State Machine](./media/lieutenant_state_machine.png)


## Design Decisions

### Preventing Faulty Processes from Blocking Non-Faulty Processes

One of the tricky issues with a synchronous communication model with rounds and
UDP communication is that faulty processes should not block forward progress of
functional processes. To guarantee this, two design decisions were made:

#### Sender Threading Model

The design of the state machine was primarily driven by the interface exposed by
the `udp::Server` and `udp::Client` classes. Both of these classes use a
synchronous execution model, which meant that to gain any concurrency, it was
necessary to do so outside their abstraction boundary. Because of this, we
decided to gain concurrency through the use of threads.

Communication to remote processes was always performed in an isolated thread.
Instead of sending messages sequentially to each process, all processes were
communicated with in parallel. This prevented a single faulty process from
splitting up functional processes because of a large timeout delay. For
instance, if the Commander sent messages to 3 Lieutenants serially, but the
second one was faulty and caused a sender timeout, the first lieutenant would
end up far ahead of the third lieutenant in the agreement algorithm.Instead,
since all communication was done in parallel, all processes stay in sync despite
the existence of fault processes, because they were all sending and receiving
messages at roughly the same time.

This was accomplished by introducing the `threadutil::ThreadGroup` class.

#### Timeouts

There were two types of timeouts used to prevent faulty processes from harming
the forward progress of working processes.

##### Reliable Communication Timeouts

When sending a message to a remote process, our communication protocol sent the
message over a UDP socket and started a timer while waiting for an
acknowledgment. If the timeout was hit before an acknowledgment was received,
it would attempt to send the message again, and would again listen for an Ack.
Up to three attempts would be made for any given message before the sender would
give up. The mechanics of this are in `udp::Client::SendWithAck`.

On the receiving side, a server would receive messages from a UDP socket. It
would first perform some cursory message validation, which if successful would
then trigger the response of an acknowledgment message. The validation included
checks like proper message formatting, logical message data, and that the host
process was who they said they were.

Note that there is a small chance that an acknowledgment for a different
message from the same remote host in the same round could be misinterpreted.
This is possible if an Ack response gets delayed until the transmission of a
different message in the same round is attempted. Because we were not able to
extend the Ack message format to include a sequence number, this could not be
prevented.

##### Round Timeouts

The agreement algorithm is synchronous and based on rounds. Therefore, in order
to assure forward progress in the face of faulty processes and asynchronous
communication channels, each round had to have a bounded time duration. This was
accomplished by a two-level round timeout scheme. First, a Lieutenant's UDP
Server socket was given a timeout so that it would never listen for longer than
a round's maximum duration. This alone was not sufficient, though, because a
malicious process could continue sending invalid messages to the non-faulty
process, which would result in the Server's `recvfrom` call being reinitialized
without making any forward progress.

To get around this, we also kept track of the start time of each round. We would
then make sure the duration between message processing this start time never
exceeded the round timeout when processing Server requests.As the comment above
`round_start_ts_` states, we used a monotonic `std::chrono::steady_clock` to
measure elapsed time accurately even in the face of clock resets, which is a
valid concern in distributed environments.

These two timeouts together meant that it was possible for a round to go
slightly over the desired timeout duration, but that a round would never exceed
twice the timeout duration. This meant that there was a strict upper bound of a
round's duration of `2*round_timeout`, which in this case is 2 seconds.

### Malicious Behavior Representation

Malicious behavior is represented using bit flags packed into a single integer
through the `MaliciousBehavior` enum class. Using bit flags means that we can
compactly represent a large number of orthogonal conditions and test each one
with a single bit mask. This bit vector is constructed on startup by reading in
all of the **--malicious** flags provided.

The `General` class then holds onto the `MaliciousBehavior` and provides
transparent access to it through a number of protected helper methods. These
methods are:

- `bool ShouldSendMsg()`: determines if a `General` should send a certain
  message, based on its malicious behavior. This will always return true if a
  `General` is not malicious, but may return false for traitors depending on the
  type of malicious behavior they exhibit.
- `void MaybeDelaySend()`: usually a no-op, but in cases of a `General` who
  exhibits delaying behavior, it may block to a random amount of time.
- `msg::Order OrderForMsg()`: determines the order to send for a message based
  on the order the `General` should send and on its malicious behavior. A loyal
  `General` will always return the correct `Order`, while a traitor may return
  an incorrect one. This is exposed on the `Commander` only for now, because we
  do not allow `Lieutenant`s to flip a message's order. The reason for this is
  that we have implemented the algorithm for Signed Messages, so we assume that
  a `Lieutenant` flipping a message's order would be detected.

By hiding the malicious behavior behind these utility methods, the rest of the
state machine for both the `Commander` and the `Lieutenant` classes could ignore
the existence of malicious behavior.


## Implementation Issues

### Multiple Processes on the Same Host

One of the implementation issues faced while developing the algorithm was its
difficulty to test, because the suggested template "assumes that each host is
running only one instance of the process." This meant that even during
development, to test a _m_ process instance of the algorithm, _m_ hosts needed
to coordinate and be kept in sync with code changes. To address this, the
single-process-per-host restriction was lifted early in the development cycle.
This was accomplished by allowing an optional port specification in the hostfile
for a given process using a `<hostname>:<port>` notation. Once individual
processes could specify unique ports, an optional **-i** (**--id**) flag was
used to distinguish the current process in a hostfile where multiple processes
were running on the same host. This way, the algorithm could be run on a single
host with a hostfile like:

```
<hostname>:1234
<hostname>:1235
<hostname>:1236
<hostname>:1237
```

And commands like:

```
./bin/general -h hostfile -f 1 -C 0 -o attack -i=0
```

and

```
./bin/general -h hostfile -f 1 -C 0 -i=1
```


## References

L. Lamport, R. Shostak, and M. Pease. [The Byzantine Generals
Problem](https://people.eecs.berkeley.edu/~luca/cs174/byzantine.pdf). ACM Trans.
Program. Lang. Syst., 4(3):382–401, July 1982.
