/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020, Intel Corporation
 */

/*
 * conn.c -- librpma connection-related implementations
 */

#include "cmocka_alloc.h"
#include "conn.h"
#include "rpma_err.h"
#include "out.h"

struct rpma_conn {
	struct rdma_cm_id *id; /* a CM ID of the connection */
	struct rdma_event_channel *evch; /* event channel of the CM ID */
	struct ibv_cq *cq; /* completion queue of the CM ID */
};

/* internal librpma API */

/*
 * rpma_conn_new -- migrate an obtained CM ID into newly created event channel.
 * If succeeded wrap provided entities into a newly created connection object.
 *
 * Note: rdma_migrate_id(3) will block if the previous event channel of the CM
 * ID has any outstanding (unacknowledged) events.
 */
int
rpma_conn_new(struct rdma_cm_id *id, struct ibv_cq *cq,
		struct rpma_conn **conn_ptr)
{
	if (id == NULL || cq == NULL || conn_ptr == NULL)
		return RPMA_E_INVAL;

	int ret = 0;

	struct rdma_event_channel *evch = rdma_create_event_channel();
	if (!evch) {
		Rpma_provider_error = errno;
		return RPMA_E_PROVIDER;
	}

	if (rdma_migrate_id(id, evch)) {
		Rpma_provider_error = errno;
		ret = RPMA_E_PROVIDER;
		goto err_destroy_evch;
	}

	struct rpma_conn *conn = Malloc(sizeof(*conn));
	if (!conn) {
		ASSERTeq(errno, ENOMEM);
		ret = RPMA_E_NOMEM;
		goto err_migrate_id_NULL;
	}

	conn->id = id;
	conn->evch = evch;
	conn->cq = cq;
	*conn_ptr = conn;

	return 0;

err_migrate_id_NULL:
	(void) rdma_migrate_id(id, NULL);

err_destroy_evch:
	rdma_destroy_event_channel(evch);

	return ret;
}

/* public librpma API */

/*
 * rpma_conn_next_event -- XXX uses rdma_get_cm_event
 */
int
rpma_conn_next_event(struct rpma_conn *conn, enum rpma_conn_event *event)
{
	return RPMA_E_NOSUPP;
}

/*
 * rpma_conn_get_private_data -- XXX
 */
int
rpma_conn_get_private_data(struct rpma_conn *conn, void **private_data,
		uint8_t *private_data_len)
{
	return RPMA_E_NOSUPP;
}

/*
 * rpma_conn_disconnect -- XXX uses rdma_disconnect
 */
int
rpma_conn_disconnect(struct rpma_conn *conn)
{
	return RPMA_E_NOSUPP;
}

/*
 * rpma_conn_delete -- delete the connection object
 */
int
rpma_conn_delete(struct rpma_conn **conn_ptr)
{
	if (conn_ptr == NULL)
		return RPMA_E_INVAL;

	struct rpma_conn *conn = *conn_ptr;
	if (conn == NULL)
		return 0;

	int ret = 0;

	rdma_destroy_qp(conn->id);

	Rpma_provider_error = ibv_destroy_cq(conn->cq);
	if (Rpma_provider_error) {
		ret = RPMA_E_PROVIDER;
		goto err_destroy_id;
	}

	if (rdma_destroy_id(conn->id)) {
		Rpma_provider_error = errno;
		ret = RPMA_E_PROVIDER;
		goto err_destroy_event_channel;
	}

	rdma_destroy_event_channel(conn->evch);

	Free(conn);
	*conn_ptr = NULL;

	return 0;

err_destroy_id:
	(void) rdma_destroy_id(conn->id);
err_destroy_event_channel:
	rdma_destroy_event_channel(conn->evch);

	Free(conn);
	*conn_ptr = NULL;

	return ret;
}
