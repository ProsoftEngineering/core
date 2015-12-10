# Core

<https://github.com/ProsoftEngineering/core>

## Testing

Tests are implemented using the [Catch](https://github.com/philsquared/Catch) framework.

Tests are required for all code. They should be extensive, striving for maximal code coverage.

Tests should prefer the Catch BDD-style WHEN()/THEN() style. (Though some existing tests do not use this style.)

To build and execute tests, run the following from this directory:

    rake

Alternative build specific targets are also available in the rakefile.

## Dependencies

While technically optional, the following make automated cross-platform testing much easier.

* ctest (Part of cmake)

* [Rake](https://github.com/ruby/rake) Ruby make
