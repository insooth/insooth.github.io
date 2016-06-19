
# Designing intrusive data types

Designing for resource-constrained systems requires more attention that usual. We need to pose "what if" question much often, ask for edge conditions and handle _all_ the cases we can anticipate. We need to understand control flow and deal with it transparently; know the resources, their owners and lifetime. We need to do multitude of things and do them with care, and at the same time keep our design _simple_, it may be complex but never complicated.

We can help ourselves at the very begining and start with design that is [testable](https://github.com/insooth/insooth.github.io/blob/master/testable-design.md "Start with testable design right now"). We can start even better and separate data from the algorithms that operate on it, as it is done in C++ STL and in the most of  functional laguages. We can do even more and move part of responsibilities directly to the caller.

Let's have a look at the selected thoughts that build idea of _intrusive_ design which applies well to the resource-constrained systems.

NOTE: This article is neither about [building intrusive list or tree](http://www.codefarms.com/publications/intrusiv/Intrusive.pdf "Intrusive Data Structures  - Jiri Soukup"), Linux kernel [lists](https://isis.poly.edu/kulesh/stuff/src/klist/ "Linux Kernel Linked List Explained"), nor [Boost](http://www.boost.org/doc/libs/1_61_0/doc/html/intrusive.html "Boost.Intrusive") or [BSD](http://www.freebsd.org/cgi/man.cgi?queue "FreeBSD Library Functions Manual - QUEUE(3)") intrusive containers.

## Data and control plane

## Resources

### Multiple users
### Non-contiguous resources

## Rich with types


#### About this document

June 19, 2016 &mdash; Krzysztof Ostrowski
