.. _changelog:

Changelog
=========

For convenience, this changelog keeps tracks of changes with version numbers
of the main cppyy package, but many of the actual changes are in the lower
level packages, which have their own releases.
See :doc:`packages <packages>`, for details on the package structure.
PyPy support lags CPython support.

master: 1.5.1
-------------

* Instantiate templates with larger integer types if argument values require
* Improve cppyy.interactive and partially enable it on PyPy, IPython, etc.
* Let __overload__ be more flexible in signature matching
* Make list filtering of dir(cppyy.gbl) on Windows same as Linux/Mac
* Extended documentation


2019-08-18: 1.5.0
-----------------

* Upgrade cppyy-cling to 6.18
* Allow python-derived classes to be used in templates
* Stricter template resolution and better caching/performance
* Detailed memory management for make_shared and shared_ptr
* Two-way memory management for cross-inherited objects
* Reduced memory footprint of proxy objects in most common cases
* Allow implicit conversion from a tuple of arguments
* Data set on namespaces reflected on C++ even if data not yet bound
* Generalized resolution of binary operators in wrapper generation
* Proper naming of arguments in namespaces for std::function<>
* Cover more cases of STL-liker iterators
* Allow std::vector initialization with a list of constructor arguments
* Consistent naming of ``__cppname__`` to ``__cpp_name__``
* Added __set_lifeline__ attribute to overloads
* Fixes to the cmake fragments for Ubuntu
* Fixes linker errors on Windows in some configurations
* Support C++ naming of typedef of bool types
* Basic views of 2D arrays of builtin types
* Extended documentation


2019-07-01 : 1.4.12
-------------------

* Automatic conversion of python functions to std::function arguments
* Fix for templated operators that can map to different python names
* Fix on p3 crash when setting a detailed exception during exception handling
* Fix lookup of std::nullopt
* Fix bug that prevented certain templated constructors from being considered
* Support for enum values as data members on "enum class" enums
* Support for implicit conversion when passing by-value


2019-05-23 : 1.4.11
-------------------

* Workaround for JITed RTTI lookup failures on 64b MS Windows
* Improved overload resolution between f(void*) and f<>(T*)
* Minimal support for char16_t (Windows) and char32_t (Linux/Mac)
* Do not unnecessarily autocast smart pointers


2019-05-13 : 1.4.10
-------------------

* Imported several FindCppyy.cmake improvements from Camille's cppyy-bbhash
* Fixes to cppyy-generator for unresolved templates, void, etc.
* Fixes in typedef parsing for template arguments in unknown namespaces
* Fix in templated operator code generation
* Fixed ref-counting error for instantiated template methods


2019-04-25 : 1.4.9
------------------

* Fix import error on pypy-c


2019-04-22 : 1.4.8
------------------

* std::tuple is now iterable for return assignments w/o tie
* Support for opaque handles and typedefs of pointers to classes
* Keep unresolved enums desugared and provide generic converters
* Treat int8_t and uint8_t as integers (even when they are chars)
* Fix lookup of enum values in global namespace
* Backported name mangling (esp. for static/global data lookup) for 32b Windows
* Fixed more linker problems with malloc on 64b Windows
* Consistency in buffer length calculations and c_int/c_uint handling  on Windows
* Properly resolve overloaded functions with using of templates from bases
* Get templated constructor info from decl instead of name comparison
* Fixed a performance regression for free functions.


2019-04-04 : 1.4.7
------------------

* Enable initializer_list conversion on Windows as well
* Improved mapping of operator() for indexing (e.g. for matrices)
* Implicit conversion no longer uses global state to prevent recursion
* Improved overload reordering
* Fixes for templated constructors in namespaces


2019-04-02 : 1.4.6
------------------

* More transparent use of smart pointers such as shared_ptr
* Expose versioned std namespace through using on Mac
* Improved error handling and interface checking in cross-inheritance
* Argument of (const/non-const) ref types support in callbacks/cross-inheritance
* Do template argument resolution in order: reference, pointer, value
* Fix for return type deduction of resolved but uninstantiated templates
* Fix wrapper generation for defaulted arguments of private types
* Several linker fixes on 64b Windows


2019-03-25 : 1.4.5
------------------

* Allow templated free functions to be attached as methods to classes
* Allow cross-derivation from templated classes
* More support for 'using' declarations (methods and inner namespaces)
* Fix overload resolution for std::set::rbegin()/rend() operator ==
* Fixes for bugs #61, #67
* Several pointer truncation fixes for 64b Windows
* Linker and lookup fixes for Windows


2019-03-20 : 1.4.4
------------------

* Support for 'using' of namespaces
* Improved support for alias templates
* Faster template lookup
* Have rootcling/genreflex respect compile-time flags (except for --std if
  overridden by CLING_EXTRA_FLAGS)
* Utility to build dictionarys on Windows (32/64)
* Name mangling fixes in Cling for JITed global/static variables on Windows
* Several pointer truncation fixes for 64b Windows


2019-03-10 : 1.4.3
------------------

* Cross-inheritance from abstract C++ base classes
* Preserve 'const' when overriding virtual functions
* Support for by-ref (using ctypes) for function callbacks
* Identity of nested typedef'd classes matches actual
* Expose function pointer variables as std::function's
* More descriptive printout of global functions
* Ensure that standard pch is up-to-date and that it is removed on
  uninstall
* Remove standard pch from wheels on all platforms
* Add -cxxflags option to rootcling
* Install clang resource directory on Windows
