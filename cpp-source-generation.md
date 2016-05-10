
# Better way to generate C++ source code

One of the points on list of [The Eight Defensive Programmer Strategies](http://c.learncodethehardway.org/book/ex27.html "Learn C The Hard Way. Exercise 27: Creative And Defensive Programming") is

> Automate everything, especially testing.

Plenty of software projects use source code generation to automate numerous tedious tasks, produce better "coder experience" (like Qt and its MOC), keep things in one place (like XLS/CSV file) and thus make management happy. Let's have a look how to resolve code generation task in much cleaner way compared to what we find usually, and make its results maintainable.

# Data flow

Source code generation _tool_ is just a data transformation utility. It consumes some read-only machine-readable (at least) input data, and produces output data in the destination format. That's very important observation that we deal with _read-only_ data, we will get back to that peculiarity in the next paragraphs.

## Toolbox

Data we want to process is typically contained in a file to which path we pass through the command-line directly to the _tool_. Following things should be considered:

* Format of the input data &mdash; it should be in well-known parsable format, otherwise we will need to spend weeks to design, implement, test and later maintain custom data format: choose one of widely-supported human-readable text formats like YAML/JSON, XML, CSV or INI.
* Binary data formats (like XLS), closed, non-human-readable or formats with questionable library support should be considered as a last resort, or not considered at all.

Generator is typically just a single script (rather binary application), choose language that easily processes input data and has stable and portable tools for doing text processing. Such language should offer basic data structures (like lists and trees) and combinators (like map and fold). Probably the best choice for old-timers is Perl, for the rest that is Python which ships with "battery included". Definitely we should not build binary application if we don't have to.

From the other perspective, language of choice shall be expressive yet easy to learn. That means C++, Haskell, Scala or C are not the best choices, even they are great languages. Ideally, all the features required to build source code generation are in the core of the language, so that no external dependencies are needed.

## Behaviour

Generator should read input data, parse it into internal in-memory representation (e.g. tree), and then without performing modifications to any of the intermediate data structures and the in-memory representation itself, provide the output data to the user. That implies we process data in a clean, pure way. We split whole processing into stages and pass all the intermediate data explicitly (as arguments) from stage to stage. Single stage is represented by a function, there must be at least:

    (1) READ INPUT -> (2) PARSE INPUT -> (3) INTERPRET INPUT -> (4) GENERATE OUTPUT -> (5) WRITE OUTPUT

All the processing shall be done in data-flow manner, each stage of processing refines the output of the previous one. Most of the functionality one needs to achieve that provide `map`, `filter`, `fold`/`reduce` utilities plus simple (unnested) `if-then-else` conditional. Raw `for` loops that are doing literally everything shall be avoided, as well as fancy global "helper" variables that destroy control flow.

Special care should be taken if processing really huge files. Probably the most reasonable option in that case is memory-mapping of the input data and building of minimal, reusable, and lazy data structures. Since we never modify once successfully created (that is filled in with proper data, do not to confuse with persistent data structures) intermediate data, we have great possibility of sharing common data (e.g. through references in Perl).

## Output representation

The result of the stage `(3)` is a data structure (usually an associative, `dict` in Python or `HASH` in Perl) that contains all the data required to generate output. Since we want to generate C++ source code, our output format is a C++ header (most likely) or source code.

Extremely important is never to mix generator code with the output format (here C++ code). Such spaghetti of scripting language mixed with strings representing destination format, unclear concatenations and accidental patching/fixing by nested `if` blocks is a sign of bad design or lack of design.

How to never try spaghetti again? Apply MVC design pattern.

# Less logic

# Verification

# Provide support

#### About this document

May ?, 2016 &mdash; Krzysztof Ostrowski

