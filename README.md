RLib provides several new abstractions for easier use of RDMA.
It is a header-only library with minimal dependices, i.e., `ibverbs`. 

Please find examples in `./examples` for how to use RLib. 
For detailed documentations or benchmark results (including code and scripts),
please check `docs`. 


## License

RLib is released under the [Apache 2.0 license](http://www.apache.org/licenses/LICENSE-2.0.html).

As RLib is the refined execution framework of DrTM+H,
if you use RLib in your research, please cite our paper:

    @inproceedings {drtmhosdi18,
        author = {Xingda Wei and Zhiyuan Dong and Rong Chen and Haibo Chen},
        title = {Deconstructing RDMA-enabled Distributed Transactions: Hybrid is Better!},
        booktitle = {13th {USENIX} Symposium on Operating Systems Design and Implementation ({OSDI} 18)},
        year = {2018},
        isbn = {978-1-939133-08-3},
        address = {Carlsbad, CA},
        pages = {233--251},
        url = {https://www.usenix.org/conference/osdi18/presentation/wei},
        publisher = {{USENIX} Association},
        month = oct,
    }
