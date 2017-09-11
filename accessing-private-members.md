# Accessing inaccessible object members


It may be hard to imagine how interesting can an ordinary class template instantiation be. For instance, is that really possible that sample code like this:

```c++
template class assigner<Mem, &A::m>;
```

has just run an algorithm that generates a set of tools to access the *private* member `m` of class `A`? Yes it is! This article explores the ideas originally published [here](http://github.com/hliberacki/cpp-member-accessor) that use explicit template instantiation to trigger custom algorithms.

## Use case


Consider class `A` with all the members private:

```c++
class A
{
    int m = 999;
    void foo() { std::cout << "foo\n"; }
};
```

Design of a class `A` does not enable it for testing in any way. Compiler refuses access to `A`'s internals, even taking a type of a member (function) pointer produces an error:

```c++
decltype(&A::m);  // error: 'int A::m' is private within this context
```

That happens, even so [`decltype`](http://en.cppreference.com/w/cpp/language/expressions#Unevaluated_expressions) operates in the unevaluated contexts &mdash; it *queries the compile-time properties of its operand*. User can define a variable of type that matches `A` member, e.g.:

```c++
using type = void (A::*)();

type f;  // compiles fine!
```

but is not allowed to reference inaccessible items in the `A` class, code like `type f = &A::foo;` generates suitable compilation error. We definitely cannot create an object that references `private` class member. But, surprisingly, there is nothing against instantiating a template that explicitly references private data. Let's try with:

```c++
template<int (A::*)> struct Y {};
```

and

```c++
Y<&A::m> y;  // implicit instantiation leads to an error
```

but

```c++
template class Y<&A::m>;  // explicit instantiation is OK!
```

We can use this property to store the member (function) pointer to the private data somewhere in the memory during template explicit instantiation. Required memory chunk must be available outside the function scope (i.e. where the explicit instantiation is located), thus it shall be `static`. We need to solve following type tetris:

```c++
template<int (A::*)>
struct Y
{
//                     v~~ passed value of type int (A::*)
    static ??? value = ???;
//         ^~~ int (A::*)
};
```

### Type description

Let's start by wrapping explicit member types into clean abstraction:

```c++
struct MemFun { using type = void (A::*)(); };  // member function pointer
struct Mem    { using type = int  (A::*);  };   // member pointer
```

that can be later lifted into an abstraction that works for any class an any member type:

```c++
template<class R, class C, class... Ts>
struct MemFun
{
    using type = R (C::*)(Ts...);
};

template<class T, class C>
struct Mem
{
    using type = T (C::*);
};
```

`Mem` or `MemFun` is passed to `Y` that accesses nested type, and expects member (function) pointer of the respective type:

```c++
template<class T, typename T::type P>
//                                 ^~~ value known at the compile-time goes here
struct Y
{
    static constexpr typename T::type value = P;
};

// usage is:

template class Y<
                  MemFun<void, A>  // want member function pointer of this type...
                , &A::foo          // ...and got it here
                >;

```

Unfortunately, trying to call stored member function pointer against object of type `A` yields well-known error:

```c++
A a;
(a.* Y<MemFun<void, A>, &A::foo>::value)();
                       ^~~ error: 'void A::foo()' is private within this context
```

We have to to find a way to access the `value` through a proxy that will not reference the inaccessible member.

### Proxied access

Connection between a proxy and `Y` must be done during `Y` template instantiation. Class template instantiation involves running constructors of all its `static` members. Having defined:

```c++
template<class T>
struct Proxy
{
    static typename T::type value;
};

// required -- we cannot left constexpr uninitailised
template<class T>
typename T::type Proxy<T>::value;
```

we want to make this possible:

```c++
A a;
(a.* Proxy<MemFun<void, A>>::value)();  // what if there is more members of the same type?
```

by doing just this:

```c++
template class Y<MemFun<void, A>, &A::foo>;
```

We will make connection through a constructor of a `static` member:


```c++
template<class T, typename T::type P>
   class Y
// ^^^^^ we hide the contents, user shall make use of a Proxy
{
    struct X { X() { Proxy<T>::value = P; } };  // does assignment in its constructor
//                   ^^^^^^^^^^^^^^^^^^^

    static X dummy;  // constructed during Y template instantiation
//  ^~~ constexpr not allowed, since references non-constexpr Proxy<T>::value
};

template<class T, typename T::type P>
typename Y<T, P>::X Y<T, P>::dummy;
```

As a result:

```c++
template class Y<MemFun<void, A>, &A::foo>;

// ...

A a;
(a.* Proxy<MemFun<void, A>>::value)();
```

calls *private* member function `A::foo`!

### Uniqueness

Astute reader probably noticed already that generating proxies for two or more members matching the same type does not lead to the expected behaviour. That is, following will overwrite the memory chunk that stores the member (function) pointer to `A::foo` with `A::bar`:

```c++
template class Y<MemFun<void, A>, &A::foo>;
template class Y<MemFun<void, A>, &A::bar>;
```

That happens because static memory chunk is stored inside the proxy parametrised by the member (function) pointer type only, and such a chunk is reused by the next assigned member (function) pointer in turn. Remedy for that is making the `Proxy` a unique type per each member (function) pointer *symbol name* (i.e. `A::foo`, `A::bar`, etc. even if their types are equal), so that the nested `static` member `value` is unique respectively too.

```c++
template<class Tag, class T>
struct Proxy
{
    static typename T::type value;
};

template<class Tag, class T>
typename T::type Proxy<Tag, T>::value;


template<class Tag, class T, typename T::type P>
class MakeProxy
{
    struct X { X() { Proxy<Tag, T>::value = P; } };

    static X dummy;
};

template<class Tag, class T, typename T::type P>
typename MakeProxy<Tag, T, P>::X MakeProxy<Tag, T, P>::dummy;
```

Sample usage:

```c++
template class MakeProxy<struct AFoo, MemFun<void, A>, &A::foo>;
template class MakeProxy<struct ABar, MemFun<void, A>, &A::bar>;
template class MakeProxy<struct AM,   Mem<int, A>,     &A::m>;
```

```c++
A a;

(a.* Proxy<struct AFoo, MemFun<void, A>>::value)();  // calls A::foo
(a.* Proxy<struct ABar, MemFun<void, A>>::value)();  // calls A::bar
(a.* Proxy<struct AM, Mem<int, A>>::value);          // accesses A::m
```

### Better user experience

It would be great if user can pass only a single type instead of a fully elaborated `Proxy` template. We need to gather the tag and type description into custom types:

```c++
struct AFoo : MemFun<void, A> {};
struct ABar : MemFun<void, A> {};
struct AM   : Mem<int, A>     {};
```

and pass it over to the variable template:

```c++
template<class T>
const auto access_v = Proxy<T, T>::value;
```

Sample usage:

```c++
template class MakeProxy<AFoo, AFoo, &A::foo>;
template class MakeProxy<ABar, ABar, &A::bar>;
template class MakeProxy<AM,   AM,   &A::m>;
```

```c++
A a;

(a.* access_v<AFoo>)();  // calls A::foo
(a.* access_v<ABar>)();  // calls A::bar
(a.* access_v<AM>);      // accesses A::m
```

### Live code

Code available on [Coliru](http://coliru.stacked-crooked.com/a/a2ceb2bf20b91eb0).
