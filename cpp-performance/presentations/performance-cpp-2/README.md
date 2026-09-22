# Performance C++ — lecture 2

A 46-slide, 75–90 minute lecture from assembly basics to modern CPU execution.
Prerequisite: basic C++; no assembly knowledge required. Examples use x86-64
Intel syntax and the Linux System V ABI. Slides include presenter notes,
click-to-reveal checkpoint answers, and primary technical references.

From the repository root:

```sh
npm run dev:2
npm run build:2
```

The static build is written to `dist/performance-cpp-2`.
The first lecture remains available through `npm run dev` and `npm run build`.
The GitHub Pages workflow also builds this lecture at `performance-cpp-2/`
under the existing site URL on the next deployment.

For the assembly inspection lab, run from this directory:

```sh
g++ -O3 -std=c++20 -S -masm=intel examples.cpp -o /tmp/cpu-examples.s
```

`examples.cpp` contains standalone functions for inspecting compiler output;
it is not a timing harness. Pipeline timing numbers in the slides describe
explicit teaching models, not measurements of a particular CPU.

The figures in `public/figures` are direct, high-resolution crops from the
supplied books: CSAPP Figure 3.2 (PDF page 208, zero-based index 207) and
Inside the Machine Figure 7-5 (printed page 148, PDF page 170). The CPU
figure also has an enlarged back-end crop for projection. Source captions
appear on the slides; the complete PDFs are not needed by the built site.
