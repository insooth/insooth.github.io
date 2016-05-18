
# Better way to generate C++ source code

One of the points on list of [The Eight Defensive Programmer Strategies](http://c.learncodethehardway.org/book/ex27.html "Learn C The Hard Way. Exercise 27: Creative And Defensive Programming") is

> Automate everything, especially testing.

which is really wise advice.

Plenty of software projects use source code generation to automate numerous tedious tasks, produce better "coder experience" (like Qt and its MOC), keep things in one fancy place (like XLS/CSV file) and thus make management happy. Let's have a look at how to resolve code generation task in much cleaner way compared to what we usually find, and make its results maintainable.

# Data flow

Source code generation _tool_ is just a data transformation utility. It consumes some read-only (at least) machine-readable input data, and produces output data in the destination format. That's very important observation that we deal with _read-only_ data, we will get back to that peculiarity in the next paragraphs.

## Toolbox

Data we want to process is typically contained in a file to which path we pass through the command-line directly to the _tool_. Following things should be considered:

* Format of the input data &mdash; it should be in well-known parsable format, otherwise we will need to spend weeks to design, implement, test and later maintain custom data format: choose one of widely-supported human-readable text formats like YAML/JSON, XML, CSV or INI.
* Binary data formats (like XLS), closed, non-human-readable or formats with questionable library support should be considered as a last resort, or not considered at all.

Generator is typically just a single script (rather binary application), so that we should choose language that easily processes input data and has stable and portable tools for doing text processing. Such language should offer basic data structures (like lists and trees) and combinators (like [map](https://en.wikipedia.org/wiki/Map_%28higher-order_function%29 "Map (higher-order function)") and [fold](https://en.wikipedia.org/wiki/Fold_%28higher-order_function%29 "Fold (higher-order function)")). Probably the best choice for old-timers is Perl, and for the rest, it is Python which ships with "battery included". Definitely we should not build binary application if we don't have to.

From the other perspective, our language of choice shall be expressive yet easy to learn. That means C++, Haskell, Scala or C are not the best choices, even they are great languages. Ideally, all the features required to build source code generation tool shall be in the core of the language, so that no external dependencies are needed.

## Behaviour

Generator should read input data, parse it into internal in-memory representation (e.g. tree), and then without performing modifications to any of the intermediate data structures and the in-memory representation itself, provide the output data to the user. That implies we process data in a clean, pure, way. We split whole processing into stages and pass all the intermediate data explicitly (as arguments) from stage to stage. Single stage is represented by a function, there must be at least defined, _Figure 1_:

    (1) READ INPUT -> (2) PARSE INPUT -> (3) INTERPRET INPUT -> (4) GENERATE OUTPUT -> (5) WRITE OUTPUT

All the processing shall be done in data-flow manner, each stage of processing refines the output of the previous one. Most of the functionality one needs to achieve that is provided by `map`, `filter`, `fold`/`reduce` utilities plus simple (unnested) `if-then-else` conditionals. Raw `for` loops that are doing literally everything shall be avoided, as well as fancy global "helper" variables that destroy simpliicity of the control flow.

Special care should be taken if processing really huge files. Probably the most reasonable option for that case is memory-mapping of the input data and building of minimal, reusable, and lazy data structures. Since we never modify once successfully created (that is filled in with proper data, do not to confuse with persistent data structures) intermediate data, we have great possibility of sharing common data (e.g. through references in Perl).

## Output representation

The result of the stage `(3)` on _Figure 1_ is a data structure (usually an associative, `dict` in Python or `HASH` in Perl) that contains all the data required to generate output. Since we want to generate C++ source code, our output format is a C++ code in a header (most likely) or source file.

Extremely important is never to mix generator code with the output format (here C++ code). Such spaghetti of scripting language mixed with strings representing destination format, unclear concatenations and accidental patching/fixing with nested `if` blocks is a sign of bad design or lack of design at all.

How to never try spaghetti again? Apply [MVC](https://en.wikipedia.org/wiki/Model%E2%80%93view%E2%80%93controller "Model–view–controller") design pattern.

# Less logic

MVC defines a split of responsibilities and a way to compose the split parts. _M_ stands for _model_, in our case it is an input data. _C_ is a _controller_, so the generator script itself. _V_ is for _view_, the way we represent processed data (on _Figure 1_ it is output of stage `(3)`). Use of MVC is ubiquitous in the web development and GUI frameworks.

In the web development, view is typically a _template_ which is a text file where certain strings have special meaning. That special meaning is for _template engine_ which loads templates, receives structured data from the user, and then substitutes those strings according to the received data. Read more about that [here](https://en.wikipedia.org/wiki/Web_template_system "Web template system").

We shall look for logic-less template engines, that means there shall be no or almost no control statements present in the template. Input data updates are strictly forbidden. Maximum allowed logic inside a template should be `for-each` and simple boolean tests, all the data must be prepared by the generator.

Python offers widespread and easy-to-learn portable logic-less template engine called [Mustache](https://mustache.github.io/ "{{mustache}} - Logic-less templates."). It has built-in support for [YAML formatted data](https://mustache.github.io/mustache.1.html "mustache - Mustache processor") and works flawlessly unless you forgot to [unescape a variable](https://mustache.github.io/mustache.5.html#Variables). Perl users should have a look at the full-blown  [Text::Template](http://search.cpan.org/~mjd/Text-Template-1.46/lib/Text/Template.pm "Text::Template - Expand template text with embedded Perl") engine.

The template itself is a C++ code with extra syntax required by the template engine. It is strongly recommended to put generated code into dedicated namespace, like `generated`, and let the user know that he/she plays with the generated code. Typically an opening lines, like the following ones, are inserted (text in `<>` is prepared by generator and filled in by engine):


    This file is generated. DO NOT edit manually.
    <generator-script-name> <generator-script-version> <generation-timestamp> <input-file-name>

Text rendered by the template engine may look awful, so that it is recommended to auto-format it with [clang-format](http://clang.llvm.org/docs/ClangFormat.html "ClangFormat") or similar tool.

# Verification

We have reached stage `(5)` &mdash; we are done. Really? Does it compile?

At stage `(3)` we shall evaluate all the required assertions against the interpreted input that must be satisfied to guarantee correctness of the result data structure. That may be not sufficient. We must never assume something will work out-of-the-box. We shall check.

The easiest way is to compile (or interpret) the generated data and look for errors. With output in C++ format, we can use  compiler from command-line, or write simple CMake script to do that for us. Verification process must be done automatically by the script before writing generated files to disk.

# Provide support

Generator script shall provide exhaustive help and options. Options must include result files location. Generator shall never automatically ovewrite already existing files, such behaviour shall be activated explicitly through an option, and attempt to overwrite explained to the user. Do not reinvent the wheel, use [Getopt::Long](http://perldoc.perl.org/Getopt/Long.html "Getopt::Long") in Perl, and [argparse](https://docs.python.org/2/howto/argparse.html "argparse") in Python to handle the command-line options.

We shall gave an opportunity to succesfully run the script with one or none command-line arguments, i.e. path to the input file (or piped stream). Remaining options (like location of the text templates and output files, etc.) should be deduced from the current location, defaults have to be hard-coded. Presence of all the required text templates must be checked. All constraints on the input data must be verified, script shall abort with a message if one of them is not satisified.

Always provide multiple verbosity levels in options that, if enabled, print on the screen (incrementally):
- simple (like `-v`): processing stages summary and general success/failure result,
- extended (like `-vv`): contents generated by template engine before saved to the files,
- trace (like `-vvv`): all the intermediate data, including parsed input data.

Generator script must be versioned, and version string should be printable via one of the options (like `--version`). It may be resonable to save the call arguments too.

# Integrate

Invoke source code generation through build system rather than follow code review cylces each time generated code and input data need update.

#### About this document

May 12, 2016 &mdash; Krzysztof Ostrowski

