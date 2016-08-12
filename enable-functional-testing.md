# Design enabled for functional testing

While white-box testing like unit testing requires full knowledge of the structure and behaviour of the tested part of code, functional testing considers only externally visible interface and the contents of the consumed and produced data. Thus, functional testing, can be seen as black-box testing, where knowledge on the data and interfaces come from the documentation, rather than through _code exploration_.

Special care should be taken to overcome degeneration of functional testing into module tests. Functional testing involves _whole system_, while module tests verify the extracted part of the system. In case of an application composed from multiple layers, we can understand _whole system_ as a single layer (then we test through interfaces to the neighbouring layers) or all the layers (the we test through interfaces to inbound and outbound layers). Is testing a part of the layer, its one subsystem, a functional test? If it involves all the remaining subsystems and interfaces to the neighbouring layers or to the environment, then it is a functional testing. But, if it is a test of an _isolated_ subsystem of a single layer, then it is a module test.

More information on building testable code and API [here](https://github.com/insooth/insooth.github.io/blob/master/testable-design.md "Start with testable design right now").

## The outer space

How to connect to the world: definition of interfaces. Passing dependencies.

## Unplug and plug in

Placement and wiring in the functional test framework. Use of intefaces.

## Automate

Do things once, use multiple times.

## Measure

Systems enabled for functional testing are easy to measure.

#### About this document

August 13, 2016 &mdash; Krzysztof Ostrowski
