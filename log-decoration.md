
# **WIP:** Decorating with a side effect

In an ideal world of software, all the side effects are isolated, and we test pure actions. Unfortunately, in a real world software, side effects are ubiquitous, and they are inherently polluting pure code. Logging is one of such overused features that leads to costly side effects (consider distributed logging DLT prevalent in automotive industry). This article describes a technique that can be used to extract side effects brought by logging, and then compose with them back in a well defined manner.

## 

[Log less, log once](https://github.com/insooth/insooth.github.io/blob/master/log-less-log-once.md)

#### About this document

January 25, 2020 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)
