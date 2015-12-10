# Core

<https://github.com/ProsoftEngineering/core>

[![Travis Status](https://travis-ci.org/ProsoftEngineering/core.svg?branch=master)](https://travis-ci.org/ProsoftEngineering/core) [![Appveyor Status](https://ci.appveyor.com/api/projects/status/pjmfq9eqmtiywxh4?branch=master&svg=true)](https://ci.appveyor.com/project/bdb/core)

Core is a library of cross platform C++11 modules and headers that adds core functionality to the standard library.

This core set of extensions powers all of Prosoft's applications.

## Brief

* u8string: Full UTF8 string class that is modeled to match std::string as much as possible.

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
* Code must be formatted using clang-format.
* New code must be licensed using the same license as existing code.
