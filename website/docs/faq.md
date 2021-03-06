---
id: faq
title: Frequently Asked Questions
sidebar_label: FAQ
---

## Why does Sorbet think this is `nil`? I just checked that it's not!

Sorbet implements a [flow-sensitive](flow-sensitive.md) type system, but there
are some [limitations](flow-sensitive.md#limitations-of-flow-sensitivity). In
particular, Sorbet does not assume a method called twice returns the same thing
each time!

See
[Limitations of flow-sensitivity](flow-sensitive.md#limitations-of-flow-sensitivity)
for a fully working example.

## It looks like Sorbet's types for the stdlib are wrong.

Sorbet uses [RBI files](rbi.md) to annotate the types for the Ruby standard
library. Every RBI file for the Ruby standard library is maintained by hand.
This means they're able to have fine grained types, but it also means that
sometimes they're incomplete or inaccurate.

The Sorbet team is usually too busy to respond to requests to fix individual
bugs in these type annotations for the Ruby standard library. That means there
are two options:

1.  Submit a pull request to fix the type annotations yourself.

    Every RBI file for the Ruby standard library lives [here][rbi-folder] in the
    Sorbet repo. Find which RBI file you need to edit, and submit a pull request
    on GitHub with the changes.

    This is the preferred option, because then every Sorbet user will benefit.

2.  Use an [escape hatch](troubleshooting.md#escape-hatches) to opt out of
    static type checks.

    Use this option if you can't afford to wait for Sorbet to be fixed and
    published. (Sorbet publishes new versions to RubyGems nightly).

If you're having problems making a change to Sorbet, we're happy to help on
Slack! See the [Community](/community) page for an invite link.

[rbi-folder]: https://github.com/sorbet/sorbet/tree/master/rbi

## What's the difference between `T.let`, `T.cast`, and `T.unsafe`?

[→ Type Assertions](type-assertions.md)

## What's the type signature for a method with no return?

```ruby
sig {void}
```

[→ Method Signatures](sigs.md)

## How should I add types to methods defined with `attr_reader`?

Sorbet has special knowledge for `attr_reader`, `attr_writer`, and
`attr_accessor`. To add types to methods defined with any of these helpers, put
a [method signature](sigs.md) above the declaration, just like any other method:

```ruby
# typed: true
class A
  extend T::Sig

  sig {returns(Integer)}
  attr_reader :reader

  sig {params(writer: Integer).returns(Integer)}
  attr_writer :writer

  # For attr_accessor, write the sig for the reader portion.
  # (Sorbet will use that to write the sig for the writer portion.)
  sig {returns(Integer)}
  attr_accessor :accessor

  sig {void}
  def initialize
    reader = T.let(0, Integer)
    writer = T.let(0, Integer)
    accessor = T.let(0, Integer)
  end
end
```

<a href="https://sorbet.run/#%23%20typed%3A%20true%0Aclass%20A%0A%20%20extend%20T%3A%3ASig%0A%0A%20%20sig%20%7Breturns(Integer)%7D%0A%20%20attr_reader%20%3Areader%0A%0A%20%20sig%20%7Bparams(writer%3A%20Integer).returns(Integer)%7D%0A%20%20attr_writer%20%3Awriter%0A%0A%20%20%23%20For%20attr_accessor%2C%20write%20the%20sig%20for%20the%20reader%20portion.%0A%20%20%23%20(Sorbet%20will%20use%20that%20to%20write%20the%20sig%20for%20the%20writer%20portion.)%0A%20%20sig%20%7Breturns(Integer)%7D%0A%20%20attr_accessor%20%3Aaccessor%0Aend">→
View on sorbet.run</a>

> Because Sorbet knows what `attr_*` methods define what instance variables, in
> `typed: strict` there are some gotchas that apply (which are the same for
> [all other uses](type-annotations.md) of instance variables): in order to use
> an instance variable, it must be initialized in the constructor, or be marked
> [nilable](nilable-types.md) (`T.nilable(...)`).
>
> (<a href="https://sorbet.run/#%23%20typed%3A%20strict%0Aclass%20A%0A%20%20extend%20T%3A%3ASig%0A%0A%20%20sig%20%7Breturns(Integer)%7D%0A%20%20attr_reader%20%3Areader%0A%0A%20%20sig%20%7Bparams(writer%3A%20Integer).returns(Integer)%7D%0A%20%20attr_writer%20%3Awriter%0A%0A%20%20%23%20For%20attr_accessor%2C%20write%20the%20sig%20for%20the%20reader%20portion.%0A%20%20%23%20(Sorbet%20will%20use%20that%20to%20write%20the%20sig%20for%20the%20writer%20portion.)%0A%20%20sig%20%7Breturns(Integer)%7D%0A%20%20attr_accessor%20%3Aaccessor%0A%0A%20%20sig%20%7Bparams(reader%3A%20Integer%2C%20writer%3A%20Integer%2C%20accessor%3A%20Integer).void%7D%0A%20%20def%20initialize(reader%2C%20writer%2C%20accessor)%0A%20%20%20%20%40reader%20%3D%20reader%0A%20%20%20%20%40writer%20%3D%20writer%0A%20%20%20%20%40accessor%20%3D%20accessor%0A%20%20end%0Aend">→
> Full example on sorbet.run</a>)

## What's the difference between `Array` and `T::Array[…]`?

`Array` is the built-in Ruby class for arrays. On the other hand,
`T::Array[...]` is a Sorbet generic type for arrays. The `...` must always be
filled in when using it.

While Sorbet implicitly treats `Array` the same as `T::Array[T.untyped]`, Sorbet
will error when trying to use `T::Array` as a standalone type.

[→ Generics in the Standard Library](stdlib-generics.md)

## How do I accept a class object, and not an instance of a class?

[→ T.class_of](class-of.md)

## How do I write a signature for `initialize`?

When defining `initialize` for a class, we strongly encourage that you use
`.void`. This reminds people instantiating your class that they probably meant
to call `.new`, which is defined on every Ruby class. Typing the result as
`.void` means that it's not possible to do anything with the result of
`initialize`.

[→ Method Signatures](sigs.md)

## How do I override `==`? What signature should I use?

Your method should accept `BasicObject` and return `T::Boolean`.

Unfortunately, not all `BasicObject`s have `is_a?`, so we have to do one extra
step in our `==` function: check whether `Object === other`. (In Ruby, `==` and
`===` are completely unrelated. The latter has to do with [case subsumption]).
The idiomatic way to write `Object === other` in Ruby is to use `case`:

```ruby
case other
when Object
  # ...
end
```

[case subsumption]: https://stackoverflow.com/questions/3422223/vs-in-ruby

Here's a complete example that uses `case` to implement `==`:

<a href="https://sorbet.run/#%23%20typed%3A%20true%0A%0Aclass%20IntBox%0A%20%20extend%20T%3A%3ASig%0A%0A%20%20sig%20%7Breturns(Integer)%7D%0A%20%20attr_reader%20%3Aval%0A%20%20%0A%20%20sig%20%7Bparams(val%3A%20Integer).void%7D%0A%20%20def%20initialize(val)%0A%20%20%20%20%40val%20%3D%20val%0A%20%20end%0A%20%20%0A%20%20sig%20%7Bparams(other%3A%20BasicObject).returns(T::Boolean)%7D%0A%20%20def%20%3D%3D(other)%0A%20%20%20%20case%20other%0A%20%20%20%20when%20IntBox%0A%20%20%20%20%20%20other.val%20%3D%3D%20val%0A%20%20%20%20else%0A%20%20%20%20%20%20false%0A%20%20%20%20end%0A%20%20end%0Aend">
  → View on sorbet.run
</a>

## I use `T.must` a lot with arrays and hashes. Is there a way around this?

`Hash#[]` and `Array#[]` return a [nilable type](nilable-types.md) because in
Ruby, accessing a Hash or Array returns `nil` if the key does not exist or is
out-of-bounds. If you would rather raise an exception than handle `nil`, use the
`#fetch` method:

```ruby
[0, 1, 2].fetch(3) # IndexError: index 3 outside of array bounds
```

## Why is `super` is untyped, even when the parent method has a `sig`?

Sorbet can't know what the "parent method" is 100% of the time. For example,
when calling `super` from a method defined in a module, the `super` method will
be deterined only once we know which class or module this module has been mixed
into. That's a lot of words, so here's an example:

<a href="https://sorbet.run/#%23%20typed%3A%20true%0Amodule%20MyModule%0A%20%20sig%20%7Breturns(Integer)%7D%0A%20%20def%20foo%0A%20%20%20%20%23%20Can't%20know%20super%20until%20we%20know%20which%20module%20we're%20mixed%20into%0A%20%20%20%20res%20%3D%20super%0A%20%20%20%20T.reveal_type(res)%0A%20%20%20%20res%0A%20%20end%0Aend%0A%0Amodule%20ParentModule1%0A%20%20sig%20%7Breturns(Integer)%7D%0A%20%20def%20foo%0A%20%20%20%200%0A%20%20end%0Aend%0A%0Amodule%20ParentModule2%0A%20%20sig%20%7Breturns(String)%7D%0A%20%20def%20foo%0A%20%20%20%20''%0A%20%20end%0Aend%0A%0Aclass%20MyClass1%0A%20%20include%20ParentModule1%0A%20%20include%20MyModule%0Aend%0A%0Aclass%20MyClass2%0A%20%20include%20ParentModule2%0A%20%20include%20MyModule%0Aend%0A%0AMyClass1.new.foo%0AMyClass2.new.foo%0A">→
View on sorbet.run</a>

To typecheck this example, Sorbet would have to typecheck `MyModule#foo`
multiple times, once for each place that method might be used from, or place
restrictions on how and where this module can be included.

Sorbet might adopt a more sophisticated approach in the future, but for now it
falls back to treating `super` as a method call that accepts anything and
returns anything.

## Does Sorbet work with Rake and Rakefiles?

Kind of, with some effort. Rake monkey patches the global `main` object (i.e.,
top-level code) to extend their DSL, which Sorbet cannot understand:

```ruby
# -- from lib/rake/dsl_definition.rb --

...

# Extend the main object with the DSL commands. This allows top-level
# calls to task, etc. to work from a Rakefile without polluting the
# object inheritance tree.
self.extend Rake::DSL
```

→
[lib/rake/dsl_definition.rb](https://github.com/ruby/rake/blob/80e00e2d59ea5b230f2f0416c387c0b57184f1ff/lib/rake/dsl_definition.rb#L192-L195)

Sorbet cannot model that a single instance of an object (in this case `main`)
has a different inheritance hierarchy than that instance's class (in this case
`Object`).

To get around this, factor out all tasks defined the `Rakefile` that should be
typechecked into an explicit class in a separate file, something like this:

```ruby
# -- my_rake_tasks.rb --

# (1) Make a proper class inside a file with a *.rb extension
class MyRakeTasks
  # (2) Explicitly extend Rake::DSL in this class
  extend Rake::DSL

  # (3) Define tasks like normal:
  task :test do
    puts 'Testing...'
  end

  # ... more tasks ...
end

# -- Rakefile --

# (4) Require that file from the Rakefile
require_relative './my_rake_tasks'
```

For more information, see
[this StackOverflow question](https://stackoverflow.com/a/60556206/1015863).

## How do I upgrade Sorbet?

Sorbet has not reached version 1.0 yet. As such, it will make breaking changes
constantly, without warning.

To upgrade a patch level (e.g., from 0.4.4314 to 0.4.4358):

```
bundle update sorbet sorbet-runtime
# also update plugins like sorbet-rails, if any
bundle exec srb init

# For plugins like sorbet-rails, see their docs, eg.
https://github.com/chanzuckerberg/sorbet-rails#initial-setup

# Optional: Suggest new, stronger sigils (per-file strictness
# levels) when possible. Currently, the suggestion process is
# fallible, and may suggest downgrading when it's not necessary.
bundle exec srb rbi suggest-typed
```

## What platforms does Sorbet support?

The `sorbet` and `sorbet-runtime` gems are currently only tested on Ruby 2.5 and
Ruby 2.6.

Ruby 2.7 has
[known issues](https://github.com/sorbet/sorbet/issues?q=is%3Aissue+2.7+) and is
[not being worked on yet](https://github.com/sorbet/sorbet/issues/2771#issuecomment-599761098)
(as of May 2020) but PRs are welcome!

The static checker is only tested on macOS 10.14 (Mojave) and Ubuntu 18 (Bionic
Beaver). We expect it to work on macOS 10.10 (Yosemite) and most Linux
distributions where `glibc`, `git` and `bash` are present.

If you are using one of the official minimal Ruby Docker images you will need to
install the extra dependencies yourself:

```Dockerfile
FROM ruby:2.6-alpine

RUN apk add --no-cache --update \
    git \
    bash \
    ca-certificates \
    wget

ENV GLIBC_RELEASE_VERSION 2.30-r0
RUN wget -nv -O /etc/apk/keys/sgerrand.rsa.pub https://alpine-pkgs.sgerrand.com/sgerrand.rsa.pub && \
    wget -nv https://github.com/sgerrand/alpine-pkg-glibc/releases/download/${GLIBC_RELEASE_VERSION}/glibc-${GLIBC_RELEASE_VERSION}.apk && \
    apk add glibc-${GLIBC_RELEASE_VERSION}.apk && \
    rm /etc/apk/keys/sgerrand.rsa.pub && \
    rm glibc-${GLIBC_RELEASE_VERSION}.apk
```

There is currently no Windows support.

## Does Sorbet support ActiveRecord (and Rails?)

[sorbet-rails] is a community-maintained project which can help generate RBI
files for certain Rails constructs.

Also see the [Community] page for more community-maintained projects!

[sorbet-rails]: https://github.com/chanzuckerberg/sorbet-rails
[community]: /en/community

## Can I convert from YARD docs to Sorbet signatures?

[Sord] is a community-maintained project which can generate Sorbet RBI files
from YARD docs.

Also see the [Community] page for more community-maintained projects!

[sord]: https://github.com/AaronC81/sord

## When Ruby 3 gets types, what will the migration plan look like?

The Sorbet team is actively involved in the Ruby 3 working group for static
typing. There are some things we know and something we don't know about Ruby 3.

Ruby 3 plans to ship type annotations for the standard library. These type
annotations for the standard library will live in separate Ruby Signature (RBS)
files, with the `*.rbs` extension. The exact syntax is not yet finalized. When
the syntax is finalized, Sorbet intends to ingest both RBS and RBI formats, so
that users can choose their favorite.

Ruby 3 has no plans to change Ruby's syntax. To have type annotations for
methods live in the same place as the method definition, the only option will be
to continue using Sorbet's [method signatures](sigs.md). As such, the Sorbet
docs will always use RBI syntax in examples, because the syntax is the same for
signatures both within a Ruby file and in external [RBI files](rbi.md).

Ruby 3 has no plans to ship a type checker for RBS annotations. Instead, Ruby 3
plans to ship a type profiler, which will attempt to guess signatures for code
without signatures. The only way to get type checking will be to use third party
tools, like Sorbet.

Ruby 3 plans to ship no specification for what the type annotations mean. Each
third party type checker and the Ruby 3 type profiler will be allowed to ascribe
their own meanings to individual type annotations. When there are ambiguities or
constructs that one tool doesn't understand, it should fall back to `T.untyped`
(or the equivalent in whatever RBS syntax decides to use for
[this construct](untyped.md)).

Ruby 3 plans to seed the initial type annotations for the standard library from
Sorbet's extensive existing type annotations for the standard library. Sorbet
already has great type annotations for the standard library in the form of
[RBI files](rbi.md) which are used to type check millions of lines of production
Ruby code every day.

From all of this, we have every reason to believe that users of Sorbet will have
a smooth transition to Ruby 3:

- You will be able to either keep using Sorbet's RBI syntax or switch to using
  RBS syntax.
- The type definitions for the standard library will mean the same (because they
  will have come from Sorbet!) but have a different syntax.
- For inline type annotations with Ruby 3, you will have to continue using
  Sorbet's `sig` syntax, no different from today.

For more information, watch [this section](https://youtu.be/2g9R7PUCEXo?t=2022)
from Matz's RubyConf 2019 keynote, which talks about his plans for typing in
Ruby 3.

## Can I use Sorbet for duck typed code?

No. You can use an [interface](abstract.md) instead, or `T.untyped` if you do
not control all of the code.

Duck typing (or, more formally, Structural typing) specifies types by their
structure. For example, Rack middleware accepts any object that has a `call`
method which takes one argument and returns a tuple representing an HTTP
response.

Sorbet does not support duck typing either for static analysis or runtime
checking.
