MessagePack-RPC IDL and code generator
======================================

## Requirements

  - ruby >= 1.8.6


## Installation

Configure and install in the usual way:

    $ ./bootstrap  # if needed
    $ ./configure
    $ make
    $ sudo make install


## Writing language binding

Add **&lt;language name&gt;.rb** file to [lang/](http://github.com/msgpack/msgpack-rpc/blob/master/gen/lang/) directory and modify [Makefile.am](http://github.com/msgpack/msgpack-rpc/blob/master/gen/Makefile.am) file.

In the file, implement **generate(doc, outdir, langdir)** method. *doc* is a AST::Document, *outdir* is the path to output directory, *langdir* is the path to lang/ directory.

See [mprpc_ast.rb](http://github.com/msgpack/msgpack-rpc/blob/master/gen/mprpc_ast.rb) file for the specification of AST module.


## License

    Copyright (c) 2010 FURUHASHI Sadayuki
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
