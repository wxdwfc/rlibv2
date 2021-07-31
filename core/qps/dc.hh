#pragma once

#include "../rmem/handler.hh"

#include "./mod.hh"
#include "./impl.hh"

namespace rdmaio {

    using namespace rmem;

    namespace qp {
        struct __attribute__((packed)) DCAttr {
            long unsigned int lid;
            u32 dct_num;
            int dc_key;
        };

        /**
         * An abstraction of DC target. When receive the `connection` request
         * sent by the DC initiator, it will create one `DCR` for it. 
         * 
         * Normally, one DC Target is sufficient for each machine.
        */
        class DCTarget {

        private:
            struct ibv_exp_dct *dct;
            struct ibv_srq *srq;
        public:
            const QPConfig my_config;

            DCTarget(Arc<RNic> nic, const QPConfig &config): my_config(config) {
                // 1. create cq
                auto res = Impl::create_cq(nic, my_config.max_send_sz());
                if (res != IOCode::Ok) {
                    RDMA_LOG(4) << "Error on creating CQ: " << std::get<1>(res.desc);
                    return;
                }
                struct ibv_cq *tmp_cq = std::get<0>(res.desc);

                // 2. create srq
                auto srq_res = Impl::create_srq(nic, my_config.max_send_sz(), 1);
                if (srq_res != IOCode::Ok) {
                    RDMA_LOG(4) << "Error on creating SRQ: " << std::get<1>(res.desc);
                    return;
                }
                this->srq = std::get<0>(srq_res.desc);

                // 3. create dct
                const int dc_key = my_config.dc_key;
                auto dct_res = Impl::create_dct(nic, 
                                tmp_cq, this->srq, dc_key);
                if (dct_res != IOCode::Ok) {
                    RDMA_LOG(4) << "Error on creating DCT: " << std::get<1>(res.desc);
                    return;
                }
                this->dct = std::get<0>(dct_res.desc);
                this->remote_dct_attr = {
                    .lid = nic->lid.value(),
                    .dct_num = dct->dct_num,
                    .dc_key = dc_key
                };
            }

        public:
            struct DCAttr remote_dct_attr;

            static Option<Arc<DCTarget>> create(Arc<RNic> nic, 
                                    const QPConfig &config) {
                auto res = Arc<DCTarget>(new DCTarget(nic, config));
                return Option<Arc<DCTarget>>(std::move(res));
            }

            ~DCTarget() {
                if (this->dct != NULL) {
                    ibv_exp_destroy_dct(this->dct);
                }

                if (this->srq != NULL) {
                    ibv_destroy_srq(srq);
                }
            }
        };

        /**
         * an abstraction of DC initiator.
        */
        class DC : public Dummy, public std::enable_shared_from_this<DC> {
        public:
            const QPConfig my_config;

            DC(Arc<RNic> nic, const QPConfig &config, ibv_cq *recv_cq = nullptr) 
                : Dummy(nic), my_config(config) {
                // 1. cq
                auto res = Impl::create_cq(nic, my_config.max_send_sz());
                if (res != IOCode::Ok) {
                    RDMA_LOG(4) << "Error on creating CQ: " << std::get<1>(res.desc);
                    return;
                }
                this->cq = std::get<0>(res.desc);

                // 2. qp
                auto res_qp = Impl::create_qp_ex(nic, config, cq, recv_cq);
                if (res_qp != IOCode::Ok) {
                    RDMA_LOG(4) << "Error on creating QP extend: " << std::get<1>(res.desc);
                    return;
                }
                this->qp = std::get<0>(res_qp.desc);

                // 3. bring status to rtr and rts
                if (!bring_dc_to_init(qp) || !bring_dc_to_rtr(qp) || 
                    !bring_dc_to_rts(qp)) {
                    RDMA_ASSERT(false);
                    this->cq = nullptr; 
                }
            }

            QPAttr my_attr() const override {
                return {.addr = nic->addr.value(),
                        .lid = nic->lid.value(),
                        .psn = static_cast<u64>(my_config.rq_psn),
                        .port_id = static_cast<u64>(nic->id.port_id),
                        .qpn = static_cast<u64>(qp->qp_num),
                        .qkey = static_cast<u64>(0)};
            }
            static Option<Arc<DC>> create(Arc<RNic> nic, const QPConfig &config) {
                auto res = Arc<DC>(new DC(nic, config));
                return Option<Arc<DC>>(std::move(res));
            }

            struct ReqDesc {
                // `op` could be IBV_EXP_WR_RDMA_READ / IBV_EXP_WR_RDMA_WRITE / IBV_EXP_WR_RDMA_ATOMIC
                ibv_exp_wr_opcode op; 
                // send flags assign the SIGNALED request
                unsigned int send_flags;
                // remote address handler
                ibv_ah *ah;
                // remote DCT target num
                u32 dct_num;
                // remote DCT access key
                int dc_key;
                // length of the request
                u32 len = 0;
            };

            struct ReqPayload {
                u64 laddr = 0; 
                u64 raddr = 0;
                mr_key_t lkey;
                mr_key_t rkey;
            };

            Result<std::string> post_send(
                    const ReqDesc &desc, const ReqPayload &payload) {
                struct ibv_exp_send_wr wr, *bad_wr;
                struct ibv_wc wc;
                struct ibv_sge sg_list;

                sg_list.length = desc.len;
                sg_list.lkey = payload.lkey;

                wr.next = NULL;
                wr.num_sge = 1;
                wr.exp_opcode = desc.op;
                wr.exp_send_flags = desc.send_flags;

                
                // remote info
                wr.wr.rdma.rkey = payload.rkey;
                wr.wr.rdma.remote_addr = payload.raddr;
                
                // dc
                wr.dc.ah = desc.ah; 
                wr.dc.dct_access_key = desc.dc_key;
                wr.dc.dct_number = desc.dct_num;
                sg_list.addr = payload.laddr;
                wr.sg_list = &sg_list;

                int err = ibv_exp_post_send(qp, &wr, &bad_wr);
                if (0 == err) {
                    return Ok(std::string(""));
                }
                return Err(std::string(strerror(errno)));
            }

            Result<ibv_wc> wait_one_comp(const double &timeout = ::rdmaio::Timer::no_timeout()) {
                Timer t;
                std::pair<int, ibv_wc> res;
                do
                {
                    // poll one comp
                    res = poll_send_comp();
                } while (res.first == 0 && // poll result is 0
                         t.passed_msec() <= timeout);
                if (res.first == 0)
                    return Timeout(res.second);
                if (unlikely(res.first < 0 || res.second.status != IBV_WC_SUCCESS))
                    return Err(res.second);
                return Ok(res.second);
            }

            static bool bring_dc_to_init(ibv_qp *qp) {
                struct ibv_exp_qp_attr qp_attr = {};
                qp_attr.qp_state = IBV_QPS_INIT;
                qp_attr.pkey_index = 0;
                qp_attr.port_num = 1;
                qp_attr.qp_access_flags = 0;
                long flags = IBV_EXP_QP_STATE | IBV_EXP_QP_PKEY_INDEX |
                                      IBV_EXP_QP_PORT | IBV_EXP_QP_DC_KEY;

                return ibv_exp_modify_qp(qp, &qp_attr, flags) == 0;
            }

            static bool bring_dc_to_rtr(ibv_qp *qp) {
                struct ibv_exp_qp_attr qp_attr = {};
                qp_attr.qp_state = IBV_QPS_RTR;
                qp_attr.path_mtu = IBV_MTU_4096;
                qp_attr.ah_attr.is_global = 0;
                qp_attr.ah_attr.port_num = 1;
                qp_attr.ah_attr.sl = 0;
                long flags = IBV_EXP_QP_STATE |
                            IBV_EXP_QP_PATH_MTU |
                            IBV_EXP_QP_AV;

                return ibv_exp_modify_qp(qp, &qp_attr, flags) == 0;
            }

            static bool bring_dc_to_rts(ibv_qp *qp) {
                struct ibv_exp_qp_attr qp_attr = {};
                qp_attr.qp_state = IBV_QPS_RTS;
                qp_attr.timeout = 50;
                qp_attr.retry_cnt = 5;
                qp_attr.rnr_retry = 5;
                qp_attr.max_rd_atomic = 16;
                long flags = IBV_EXP_QP_STATE | IBV_EXP_QP_TIMEOUT | IBV_EXP_QP_RETRY_CNT |
                                      IBV_EXP_QP_RNR_RETRY | IBV_EXP_QP_MAX_QP_RD_ATOMIC;
                return ibv_exp_modify_qp(qp, &qp_attr, flags) == 0;
            }

            ~DC() {
                if (this->qp != NULL) {
                    int rc = ibv_destroy_qp(qp);
                    RDMA_VERIFY(WARNING, rc == 0)
                        << "Failed to destroy QP " << strerror(errno);
                }
            }
        };

} // namespace qp

}// namespace rdmaio