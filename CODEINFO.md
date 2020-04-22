# Understanding BaaS code

BaaS is written in C++ on top of Chromium Embedded Framework (CEF), using the Poco library for the HTTP server and zlib and libjpeg-turbo for image compression. CEF/Chromium does most of the heavy lifting, and as such the code is not really performance critical. Thus safety and simplicity is prioritized over efficiency. Some conventions:

- Most objects are reference counted; to handle the boilerplate of enforcing that shared_ptr is used to manage the lifetime and disabling copy and move constructors/assignment operators, we use the SHARED_ONLY_CLASS macro defined in common.hpp.

  - SHARED_ONLY_CLASS objects are constructed by calling the static create method; the arguments are forwarded to the constructor, along with a private CKey dummy value. To obtain a shared_ptr to itself, the object may implement the afterConstruct_ member function, which is called immediately after the constructor.

  - To help avoid reference cycles, in debug builds we check at the end of the program that all SHARED_ONLY_CLASS objects have been destructed.

- The CHECK macro of CEF should be used abundantly to check assumptions made in the code. These checks are enabled also in release mode, just to be safe.

- Typically the service-centric objects (such as Server, Session, HTTPServer and UI element objects) form a tree structure, in which the parents have shared_ptrs to their children and call their member functions directly. Information flow to the opposite direction, where a child notifies or queries its parent, is typically implemented by having the parent implement an event handler interface of the child and giving the child a weak pointer to the parent. To avoid re-entrancy issues (where a function indirectly calls itself recursively by accident), the child should call the event handler functions using postTask in common.hpp so that they are called from the event loop.

- Most of the code runs in the CEF browser process UI thread (in debug builds we often check this by calling CEF_REQUIRE_UI_THREAD). To call functions in the UI thread from other threads, the postTask function in common.hpp should be used. The use of other threads is typically isolated to individual modules:

  - In http.hpp, we use the Poco thread pool for writing the responses to avoid IO blocking the UI thread. While the actual request handlers are called in the CEF UI thread, and communication with the Poco thread is handled behind the scenes. Only the callback function that writes the response body supplied by the request handler needs to be safe to call in a non-CEF UI thread.

- Even though most of our code runs in the CEF UI thread, CEF might hold shared pointers to our objects in other threads and thus it is possible that our objects are destructed outside the CEF UI thread. Therefore destructors should not directly call other objects that expect to be called in CEF UI thread (without postTask). Typically we keep our destructors as simple as possible.
