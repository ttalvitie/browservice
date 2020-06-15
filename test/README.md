# Browservice test procedure

Each supported client is tested manually using the procedure in `index.html`. To use the procedure, start Browservice, connect to it using the client and navigate to `index.html` hosted by a HTTP server. Follow the instructions on the page, and document all deviations.

One easy way to set up a HTTP server is to run the Python builtin HTTP server in this directory:

```
python3 -m http.server
```

This will listen on all interfaces for connections on port 8000. If the HTTP server is running on the same machine as Browservice, you can access the server in the URL `http://localhost:8000/` (it might make sense to use this as the initial page using the `--start-page` flag for Browservice).
