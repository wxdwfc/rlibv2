#pragma once

#include "../rmem/handler.hh"

#include "./mod.hh"
#include "./impl.hh"

#define HRD_MOD_ADD(x, N) do { \
    x = x + 1; \
    if(x == N) { \
        x = 0; \
    } \
} while(0)


namespace rdmaio {

    using namespace rmem;

    namespace qp {
        /* Info published by the DCT target */
        struct __attribute__((packed)) DCAttr {
            int lid;
            int dct_num;
            int dc_key;
        };

        /* Get the LID of a port on the device specified by @ctx */
        static inline uint16_t hrd_get_local_lid(struct ibv_context *ctx, int dev_port_id) {
            assert(ctx != NULL && dev_port_id >= 1);

            struct ibv_port_attr attr;
            if (ibv_query_port(ctx, dev_port_id, &attr)) {
                printf("HRD: ibv_query_port on port %d of device %s failed! Exiting.\n",
                       dev_port_id, ibv_get_device_name(ctx->device));
                assert(false);
            }

            return attr.lid;
        }

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

            DCTarget(Arc<RNic> nic, int dc_key) {
                // for DC target
                struct ibv_pd *pd = nic->get_pd();
                struct ibv_context *ctx = nic->get_ctx();

                struct ibv_cq *tmp_cq = ibv_create_cq(ctx, 128, NULL, NULL, 0);

                /* Create SRQ. SRQ is required to init DCT target, but we don't used it */
                struct ibv_srq_init_attr attr;
                memset((void *) &attr, 0, sizeof(attr));
                attr.attr.max_wr = 100;
                attr.attr.max_sge = 1;
                this->srq = ibv_create_srq(pd, &attr);

                /* Create one DCT target per port */
                {
                    const uint8_t port = 1;
                    struct ibv_exp_dct_init_attr dctattr;
                    memset((void *) &dctattr, 0, sizeof(dctattr));
                    dctattr.pd = pd;
                    dctattr.cq = tmp_cq;
                    dctattr.srq = srq;
                    dctattr.dc_key = dc_key;
                    dctattr.port = port;
                    dctattr.access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;
                    dctattr.min_rnr_timer = 2;
                    dctattr.tclass = 0;
                    dctattr.flow_label = 0;
                    dctattr.mtu = IBV_MTU_4096;
                    dctattr.pkey_index = 0;
                    dctattr.hop_limit = 1;
                    dctattr.create_flags = 0;
                    dctattr.inline_size = 60;

                    this->dct = ibv_exp_create_dct(ctx, &dctattr);
                    assert(this->dct != NULL);
                    /* Publish DCT info */
                    this->remote_dct_attr.lid = hrd_get_local_lid(ctx, port);
                    this->remote_dct_attr.dct_num = dct->dct_num;
                    this->remote_dct_attr.dc_key = dc_key;
                }
            }

        public:
            struct DCAttr remote_dct_attr;

            static Option<Arc<DCTarget>> create(Arc<RNic> nic) {
                auto res = Arc<DCTarget>(new DCTarget(nic, 0x01));
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


        class DC {
        public:
            struct ibv_qp *qp;
            struct ibv_cq *cq;

            DC(Arc<RNic> nic) {
                // for DC target
                struct ibv_pd *pd = nic->get_pd();
                assert(pd != NULL);
                struct ibv_context *ctx = nic->get_ctx();
                assert(ctx != NULL);


                cq = ibv_create_cq(ctx, 128, NULL, NULL, 0);
                assert(cq != NULL);
                /* Create the DCT initiator */
                struct ibv_qp_init_attr_ex create_attr;
                memset((void *) &create_attr, 0, sizeof(create_attr));
                create_attr.send_cq = cq;
                create_attr.recv_cq = cq;
                create_attr.cap.max_send_wr = 512;
                create_attr.cap.max_send_sge = 1;
                create_attr.cap.max_inline_data = 128;
                create_attr.qp_type = IBV_EXP_QPT_DC_INI;
                create_attr.pd = pd;
                create_attr.comp_mask = IBV_QP_INIT_ATTR_PD;

                qp = ibv_create_qp_ex(ctx, &create_attr);
                assert(qp != NULL);

                /* Modify QP to init */
                struct ibv_exp_qp_attr modify_attr;
                memset((void *) &modify_attr, 0, sizeof(modify_attr));
                modify_attr.qp_state = IBV_QPS_INIT;
                modify_attr.pkey_index = 0;
                modify_attr.port_num = 1;
                modify_attr.qp_access_flags = 0;

                if (ibv_exp_modify_qp(qp, &modify_attr,
                                      IBV_EXP_QP_STATE | IBV_EXP_QP_PKEY_INDEX |
                                      IBV_EXP_QP_PORT | IBV_EXP_QP_DC_KEY)) {
                    fprintf(stderr, "Failed to modify QP to INIT\n");
                    assert(false);
                }

                /* Modify QP to RTR */
                modify_attr.qp_state = IBV_QPS_RTR;
                modify_attr.max_dest_rd_atomic = 0;
                modify_attr.path_mtu = IBV_MTU_4096;
                modify_attr.ah_attr.is_global = 0;

                /* Initially, connect to the DCT target on machine 0 */
                modify_attr.ah_attr.port_num = 1;    /* Local port */
                modify_attr.ah_attr.sl = 0;

                if (ibv_exp_modify_qp(qp, &modify_attr, IBV_EXP_QP_STATE |
                                                        //IBV_EXP_QP_MAX_DEST_RD_ATOMIC | IBV_EXP_QP_PATH_MTU |
                                                        IBV_EXP_QP_PATH_MTU |
                                                        IBV_EXP_QP_AV)) {
                    fprintf(stderr, "Failed to modify QP to RTR\n");
                    assert(false);
                }

                /* Modify QP to RTS */
                modify_attr.qp_state = IBV_QPS_RTS;
                modify_attr.timeout = 14;
                modify_attr.retry_cnt = 7;
                modify_attr.rnr_retry = 7;
                modify_attr.max_rd_atomic = 16;
                if (ibv_exp_modify_qp(qp, &modify_attr,
                                      IBV_EXP_QP_STATE | IBV_EXP_QP_TIMEOUT | IBV_EXP_QP_RETRY_CNT |
                                      IBV_EXP_QP_RNR_RETRY | IBV_EXP_QP_MAX_QP_RD_ATOMIC)) {
                    fprintf(stderr, "Failed to modify QP to RTS\n");
                    assert(false);
                }
            }

        public:
            static Option<Arc<DC>> create(Arc<RNic> nic) {
                auto res = Arc<DC>(new DC(nic));
                return Option<Arc<DC>>(std::move(res));
            }

            int post_send(
                    ibv_exp_wr_opcode op, int payload_sz,
                    int lkey, uint64_t laddr, ibv_ah *ah,
                    int rkey, uint64_t raddr, DCAttr *remote_dct_attr
            ) {
                struct ibv_exp_send_wr wr, *bad_wr;
                struct ibv_wc wc;
                struct ibv_sge sg_list;

                wr.next = NULL;
                wr.num_sge = 1;
                wr.exp_opcode = op;
                wr.exp_send_flags = IBV_EXP_SEND_SIGNALED;

                sg_list.length = payload_sz;
                sg_list.lkey = lkey;

                /* Remote */
                wr.wr.rdma.rkey = rkey;

                /* Routing */
                wr.dc.ah = ah;      // via remote
                wr.dc.dct_access_key = remote_dct_attr->dc_key;
                wr.dc.dct_number = remote_dct_attr->dct_num;

                wr.wr.rdma.remote_addr = raddr;
                sg_list.addr = laddr;
                wr.sg_list = &sg_list;

                int err = ibv_exp_post_send(qp, &wr, &bad_wr);
                if (err != 0) {
                    fprintf(stderr, "Failed to post send. Error = %d\n", err);
                    assert(false);
                }
                return 0;
            }

            int poll_comp() {
                struct ibv_wc wc;
                int ret;
                while (1) {
                    ret = ibv_poll_cq(this->cq, 1, &wc);
                    if (ret == 1) {
                        assert(wc.status == IBV_WC_SUCCESS);
                        break;
                    }
                }
                return ret;
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