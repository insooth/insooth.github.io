# Design enabled for functional testing

While white-box testing like unit testing requires full knowledge of the structure and behaviour of the tested part of code, functional testing considers only externally visible interfaces and the contents of the consumed and produced data. Thus, functional testing, can be seen as black-box testing, where knowledge of the data and interfaces come directly from the documentation, rather than through lengthy _code exploration_ and guessing the author's idea.

Special care should be taken to overcome degeneration of functional testing into module tests. Functional testing involves _whole system_, while module tests verify the extracted part of the system. In case of an application composed from multiple layers, we can understand _whole system_ as a single layer (then we test through interfaces to the neighbouring layers) or all the layers (the we test through interfaces to inbound and outbound layers). Is testing a part of the layer, its one subsystem, a functional test? If it involves all the remaining subsystems and interfaces to the neighbouring layers or to the environment, then it is a functional test. But, if it is a test of an _isolated_ subsystem of a single layer, then it is a module test.

This article treats about modelling system design enabled by-default for functional testing. It does not contain hints and tips how does API and the data shall look like. More information on building testable code and API can be found [in the related article](https://github.com/insooth/insooth.github.io/blob/master/testable-design.md "Start with testable design right now").

## The outer space

In system design there are typically two words: we and _the others_. Term "we" is understood as our system that we design, the remaining part, _the others_, are systems we cooperate with or the end user (a human) that uses us. All those worlds are connected and exchange data. Connection is done through predefined interfaces (software or physical). Data consumed and produced by the interfaces is well defined in the documentation. Documentation defines a set of black boxes with limited responsibilities and protocols between them. We define sequences of actions required to achieve certain result with no digging into details. Such way of designing is known in functional programming world as _wholemeal programming_ or, more generally, very popular top-down approach.

In integrated circuit design engineers strive to produce a black box (literally) with a set of named pins, where each pin impacts the black box in a well defined way. Pins are then connected to the bigger board that is served to the end user. Through analogy to the software design, our named pins are interfaces we have to defined (less is better) and the current is the data (minimalistic; as messages (objects of certain type) or as a stream of bytes or objects).

## Unplug and plug in

Placement and wiring in the functional test framework. Use of intefaces.

## Automate

Do things once, use multiple times.

## Measure

Systems enabled for functional testing are easy to measure.

#### About this document

August 13, 2016 &mdash; Krzysztof Ostrowski
