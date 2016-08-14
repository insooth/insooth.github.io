# Design enabled for functional testing

While white-box testing like unit testing requires full knowledge of the structure and behaviour of the tested part of code, functional testing considers only externally visible interfaces and the contents of the consumed and produced data. Thus, functional testing, can be seen as black-box testing, where knowledge of the data and interfaces come directly from the documentation, rather than through lengthy _code exploration_ and guessing the author's idea.

Special care should be taken to overcome degeneration of functional testing into module tests. Functional testing involves _whole system_, while module tests verify the extracted part of the system. In case of an application composed from multiple layers, we can understand _whole system_ as a single layer (then we test through interfaces to the neighbouring layers) or all the layers (the we test through interfaces to inbound and outbound layers). Is testing a part of the layer, its one subsystem, a functional test? If it involves all the remaining subsystems and interfaces to the neighbouring layers or to the environment, then it is a functional test. But, if it is a test of an _isolated_ subsystem of a single layer, then it is a module test.

This article treats about modelling system design enabled by-default for functional testing. It does not contain hints and tips how does API and the data shall look like. More information on building testable code and API can be found [in the related article](https://github.com/insooth/insooth.github.io/blob/master/testable-design.md "Start with testable design right now").

## The outer space

In system design there are typically two worlds: we and _the others_. Term "we" is understood as our system that we design, the remaining part, _the others_, are systems we cooperate with or the end user (like a human) that uses us. All those worlds are connected and exchange data. Connection is done through predefined interfaces (software or physical). Data consumed and produced by the interfaces is well defined in the documentation. Documentation defines a set of black boxes with limited responsibilities and protocols between them. We define sequences of actions required to achieve certain result with no digging into details. Such way of designing is known in functional programming world as _wholemeal programming_ or, more generally, very popular top-down approach.

In integrated circuit design engineers strive to produce a black box (literally) with a set of named pins, where each pin impacts the black box in a well defined way. Pins are then connected to the bigger board that is served to the end user. Through analogy to the software design, our named pins are interfaces we have to defined (less is better) and the current is the data (minimalistic; as messages (objects of certain type) or as a stream of bytes or objects).

## Unplug and plug in

Our system implements an interface and provides an interface to be implemented by users. We would like to unplug our system from its original environment and plug it back into an artificial environment of functional test framework. We are interested in intercepting the data that flows from environment to the system and from the system to the environment. By calling interface actions with proper data in arguments we want to observe the results returned back to the environment. And, if system calls an action that is implemented by environment, we need to return correct results. That means, we have to know what to return in such cases, which, in turn, may involve understanding of the underlying state machine if action opens a session.

Consider an interface that provides a way to find bank name by account number. User enters account 
number and calls respective system interface, system returns back to the user with the bank name. Functional test framework is neither user nor system, i.e. it is in the middle and acts as more or less as a spy. By use of framework we want to:
* trigger interface that looks for bank name with the prepared data,
* intercept the result data before it is returned to the user.

It is easy to trigger lookup by just calling interface exposed by the system. In order to intercept the results we need to have system enabled for functional testing by design, i.e. it must be possible to inject a kind of a listener that silently intercepts results transparently (without affecting the original behaviour) and passes it to the functional test framework.

### Subscriber-publisher

TODO: Make it possible to attach spies; derive default listener.

## Automate

Do things once, use multiple times.

## Measure

Systems enabled for functional testing are easy to measure.

#### About this document

August 13, 2016 &mdash; Krzysztof Ostrowski
