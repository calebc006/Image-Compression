# QOI Encode/Decode
`QOIConverter.cpp` implements the Quite-OK-Image compression scheme (v1.0) based on [this specification](https://qoiformat.org/qoi-specification.pdf). 

Introduced by Dominic Szablewski in 2021, QOI is a simple encoding scheme making use of repeated blocks of similar pixels, in particular leveraging common ways in which nearby pixels differ.

TODO:
- [ ] Refactor to Python functional implementation (for commonality with `/arithmetic-coding`)
- [ ] Support 4-channel RGBA
- [ ] Optimize
