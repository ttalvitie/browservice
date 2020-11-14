# Understanding Browservice code

The server side of Browservice is written in C++ on top of Chromium Embedded Framework (CEF), using the Poco library for the HTTP server, zlib and libjpeg-turbo for image compression, Pango and FreeType2 for text rendering and XCB for clipboard handling. A simple custom GUI widget system (used for the browser control bar) and a multithreaded PNG compressor (for reduced compression latency) have been implemented for this project.

The client side is implemented in HTML and JavaScript. As compatibility with old browsers is a priority for the project, great care is taken to test that the essential features work for all of the supported client browsers (listed in `README.md`). Testing is done manually; to help with this, `test/index.html` contains a checklist of features that need to work (see `test/README.md` for further information). It is impractical to thoroughly test all supported client browsers for all minor changes, and thus the amount of testing must be proportional to the assessed risk of breaking compatibility. Contributors cannot be expected to test all the client browsers before submitting a pull request.

CEF/Chromium and the client browser do most of the heavy lifting, and thus our code is not really performance critical (one exception to this is the parallel PNG compressor). Thus safety, simplicity and compatibility is prioritized over efficiency in most of the code.

Some conventions and notes about the server-side code:

- Most objects are reference counted; to handle the boilerplate of enforcing that `shared_ptr` is used to manage the lifetime and disabling copy and move constructors/assignment operators, we use the `SHARED_ONLY_CLASS` macro defined in `common.hpp`.

  - `SHARED_ONLY_CLASS` objects are constructed by calling the static create method; the arguments are forwarded to the constructor, along with a private `CKey` dummy value. To obtain a `shared_ptr` to itself, the object may implement the `afterConstruct_` member function, which is called immediately after the constructor.

  - To help avoid reference cycles, in debug builds we check at the end of the program that all `SHARED_ONLY_CLASS` objects have been destructed.

- The `CHECK` macro of CEF is used abundantly to check assumptions made in the code. These checks are enabled also for release builds, just to be safe.

- Typically the service-centric objects (such as `Server`, `Session`, `HTTPServer` and UI widgets) form a tree structure, in which the parents have `shared_ptr`s to their children and call their member functions directly. Information flow to the opposite direction, where a child notifies or queries its parent, is typically implemented by having the parent implement an event handler interface of the child and giving the child a weak pointer to the parent. To avoid re-entrancy issues (where a function indirectly calls itself recursively by accident and does not take this possibility into account), the child should call the event handler functions using `postTask` in `common.hpp` so that they are called from the event loop; exceptions to this (either for performance reasons or avoiding race conditions) are documented in the event handler classes.

- We avoid relying on inheritance (apart from pure interfaces) as it tends to make the code hard to understand. We make an exception for `Widget`s, because in that case, implementing the common `Widget` behavior such as event routing and render handling in the common base `Widget` class significantly reduces the amount of boilerplate code in each widget (each widget only has to implement a few protected virtual functions to fill in the behavior specific to it).

- Most of the code runs in the CEF browser process UI thread event loop (in debug builds we often check this using the macro `REQUIRE_UI_THREAD`). To run functions in the UI thread from other threads, the `postTask` function in `common.hpp` should be used. The use of other threads is typically isolated to individual modules:

  - In `http.hpp`, we use the Poco thread pool for writing the responses to avoid IO blocking the UI thread. The actual request handlers are called in the CEF UI thread, and communication with the Poco thread is handled behind the scenes. Only the callback function that writes the response body supplied by the request handler needs to be safe to call in a non-CEF UI thread.

  - In `image_compressor.hpp` and `png.hpp`, we use worker threads to handle image compression as it is quite CPU intensive, and we want the UI to keep running at the same time.

  - In `xwindow.hpp`, we use a worker thread to handle X11 events received through XCB.

- Even though most of our code runs in the CEF UI thread, CEF might hold shared pointers to our objects in other threads and thus it is possible that our objects are destructed outside the CEF UI thread. Therefore destructors should not directly call functions of other objects that expect to be called in CEF UI thread (without `postTask`). Typically we keep our destructors as simple as possible.

- We try to keep error handling as simple as possible: most errors simply result in aborting the program (typically through the `CHECK` macro). For errors that may happen in normal use, we try to recover from the error and optionally log a message using the `INFO_LOG`, `WARNING_LOG` and `ERROR_LOG` macros in defined in `common.hpp`. We follow the CEF/Chrome convention of not using exceptions; however, we must keep in mind that Poco might throw exceptions.

- We use code generation for HTML templates (in the `html` directory). They are compiled into C++ functions (in `gen/html.cpp`) by the Makefile using the Python script `gen_html_header.py` that implements a very simple templating language (`%-VAR-%` is replaced by the value of `VAR` in the struct specified for each function in html.hpp without escaping).
