
# Phantom lambda type, partially applied class template, and deduction guides to support dependency injection

This article is a follow-up to the article [_Creating testable interfaces_](https://github.com/hliberacki/hliberacki.github.io/blob/master/testable_interfaces.md) written by Hubert Liberacki, and the highly-metaprogrammed ideas the author shared with me through live coding session.

## Making phantom types

https://wiki.haskell.org/Phantom_type
https://en.cppreference.com/w/cpp/language/aggregate_initialization
Aggregate init of base classes

## How to partially apply a class template

Carry an additional information within the type
Nested class template to the rescue
https://en.cppreference.com/w/cpp/language/type_alias
Alias template supports only one level of nesting: size of a stack of parametrised types introduces by alias template is equal to one

## Deduction guides cannot be _partially applied_

CTADs
https://en.cppreference.com/w/cpp/language/type_alias

## Dependency injection with parametrised types

hi-perf dependency injection
[_Start with testable design right now_](https://github.com/insooth/insooth.github.io/blob/master/testable-design.md)


## Potential issue

Relation between tag and a member function under test is not checked. That is, it is just good will of the user that `Foo` identifies `foo` member function. There is no explicit compile-time rule that prevents from assignment of `bar` to `Foo` tag.

We need to associate tag with a particular member function signature, and verify that during mock construction.


#### About this document

October 29, 2019 &mdash; Krzysztof Ostrowski

[LICENSE](https://github.com/insooth/insooth.github.io/blob/master/LICENSE)

