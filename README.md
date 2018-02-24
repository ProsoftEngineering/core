# Core

<https://github.com/ProsoftEngineering/core>

[![Travis Status](https://travis-ci.org/ProsoftEngineering/core.svg?branch=master)](https://travis-ci.org/ProsoftEngineering/core) [![Appveyor Status](https://ci.appveyor.com/api/projects/status/pjmfq9eqmtiywxh4?branch=master&svg=true)](https://ci.appveyor.com/project/bdb/core) [![Codecov Status](https://codecov.io/github/ProsoftEngineering/core/coverage.svg?branch=master)](https://codecov.io/github/ProsoftEngineering/core)

Core is a library of cross platform C++11 modules and headers that adds core functionality to the standard library.

This core set of extensions powers all of Prosoft's applications.

## Brief

* filesystem: Implementation of C++17 filesystem, with extensions for: ACLs, extended attributes, alternate data streams, and more.

* u8string: Full UTF8 string class that is modeled to match std::string as much as possible.

* identity: Login account information.

* string: various template string algorithms and conversions from/to all Unicode variants and platform natives (e.g. CFString/NSString)

* regex: Modeled after std::regex but powered by [Onigurama](https://github.com/kkos/oniguruma) so it's fully Unicode aware. In addition it builds with GCC 4.8 which does not provide a std::regex implementation.

## License

Modified BSD. See the LICENSE.txt file.

## Requirements

### Build

* Management
	* [CMake](https://cmake.org) 3.1+

* Compilers
	* Xcode 6+
	* Clang 3.3+
	* GCC 4.8.2+
	* Microsoft Visual Studio 2015
	* Mingw-w64

* Architectures
	* x86 64/32
	* ARM
	* PPC

### Runtime

* The following platforms are supported. All others (iOS, etc.) are unsupported.
	* Mac OS X 10.7+
	* Windows 7+
	* Modern x86 Linux Distributions (e.g. Ubuntu 14.04+)
	* FreeBSD 10+ *
	
\* *Tested but not used by Prosoft in production.*

## Contributions

We welcome any contribution, especially for platform support not already provided. However, there are a few conditions that must be met:

* All code must have accompanying tests AND pass all integration tests for all platforms.
* Code must be formatted using clang-format and adhere to the conventions set forth in Code Style.
* New code must be licensed using the same license as existing code.

## Code Style

In addition to the the style enforced by clang-format, the following conventions should be followed:

* Variable definitions should be one per line. Example :

		int foo = 0;
		int bar = 1;
		
	Not:
	
		int foo = 0, bar = 1;


* 'const' should be used for all variables and member functions whenever possible. Example:

		const int foo_that_never_changes = ...;
		
	Not:
	
		int foo_that_never_changes = ...;

* Implementation details that must occur in a header should be declared in a module specific sub-namespace that is likely to be unique. This avoids potential ambiguity when partial namespace resolution is used. Prefixing the module name with an 'i' is a good example. Example:

		namespace prosoft {
			namespace istring {
				// implementation details here
			}
		}
		
	Not:
		
		namespace prosoft {
			namespace detail {
			}
			// or
			namespace impl {
			}
		}

* Implementation details that occur in a module implementation file, should use an anonymous namespace and not the 'static' keyword. Example:
	
		namespace {
			const auto detail = ...;
		}

	Not:
		
		static const auto detail = ...;

