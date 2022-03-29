# HammerSlide
This repo contains the implementation of **HammerSlide**, an extension of the Two-Stacks 
algorithm. **Hammerslide** can be used for _incremental window aggregation_ and requires associative 
operators and data to arrive in-order (FIFO algorithm).

The algorithm requires external coordination for performing operations on top of windows 
(e.g., a slicing technique like _Panes_ or _Pairs_). It is important to perform the insertion/eviction
operations based on the window semantics to get correct results.

## Implementation

Hammerslide uses a _fixed-size circular buffer_ to store the input data, a
_variable_ to store the aggregate values of the back stack and an _array_ for the aggregate values 
of the front stack. During the swap operation of the Two-Stacks algorithm, Hammerslide reads data 
from the end to the start of the circular buffer and stores one single aggregate value per window slide
in the respective position of the _array_ with the aggregates. In addition, no input values are copied from the back to the 
front stack. For eviction, only the pointers to the circular buffer and the array are updated and new input
data are going to overwrite the old data.


## TODO
* Generalize the current solution. Only **MIN** and **SUM** aggregate functions are implemented with SIMD instructions.
* Implement `time-based` windows.
* The current implementation is based on the assumption that the input is of type `int` and that
the CPU supports `AVX` instructions.

## Dependencies
Install the following:
```
sudo apt install libtbb-dev
```
and compile with GNU c++ compiler. 

## API
```
insert(T)
insert(T *, start, end) // bulk insertion that takes advantage of SIMD instructions
evict(numberOfItems = 1)
query(isSIMD = true)    // perform swap with SIMD instructions or not
```

### How to cite HammerSlide
* **[ADMS]** Georgios Theodorakis, Alexandros Koliousis, Peter R. Pietzuch, and Holger Pirk. Hammer Slide: Work- and CPU-efficient Streaming Window Aggregation, ADMS, 2018
```
@inproceedings{Theodorakis2018HammerSW,
  title={Hammer Slide: Work- and CPU-efficient Streaming Window Aggregation},
  author={G. Theodorakis and A. Koliousis and Peter R. Pietzuch and H. Pirk},
  booktitle={ADMS@VLDB},
  year={2018}
}
```

#### Other related publications
* **[EDBT]** Georgios Theodorakis, Peter R. Pietzuch, and Holger Pirk. SlideSide: A fast Incremental Stream Processing Algorithm for Multiple Queries, EDBT, 2020
* **[SIGMOD]** Alexandros Koliousis, Matthias Weidlich, Raul Castro Fernandez, Alexander Wolf, Paolo Costa, and Peter Pietzuch. Saber: Window-Based Hybrid Stream Processing for Heterogeneous Architectures, SIGMOD, 2016
* **[SIGMOD]** Georgios Theodorakis, Alexandros Koliousis, Peter R. Pietzuch, and Holger Pirk. LightSaber: Efficient Window Aggregation on Multi-core Processors, SIGMOD, 2020
* **[VLDB]** Georgios Theodorakis, Fotios Kounelis, Peter R. Pietzuch, and Holger Pirk. Scabbard: Single-Node Fault-Tolerant Stream Processing, VLDB, 2022
