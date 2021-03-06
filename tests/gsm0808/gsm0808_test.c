/*
 * (C) 2012 by Holger Hans Peter Freyther
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <osmocom/gsm/gsm0808.h>
#include <osmocom/gsm/gsm0808_utils.h>
#include <osmocom/gsm/protocol/gsm_08_08.h>
#include <osmocom/gsm/protocol/gsm_08_58.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/application.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define EXPECT_ENCODED(hexstr) do { \
		const char *enc_str = msgb_hexdump(msg); \
		printf("%s: encoded: %s(rc = %u)\n", __func__, enc_str, rc_enc); \
		OSMO_ASSERT(strcmp(enc_str, hexstr " ") == 0); \
		OSMO_ASSERT(rc_enc == msg->len); \
	} while(0)

#define VERIFY(msg, data, data_len) do { \
		if (!msgb_eq_l3_data_print(msg, data, data_len)) \
			abort(); \
	} while(0)

/* Setup a fake codec list for testing */
static void setup_codec_list(struct gsm0808_speech_codec_list *scl)
{
	memset(scl, 0, sizeof(*scl));

	scl->codec[0].pi = true;
	scl->codec[0].tf = true;
	scl->codec[0].type = GSM0808_SCT_FR3;
	scl->codec[0].cfg = 0xcdef;

	scl->codec[1].fi = true;
	scl->codec[1].pt = true;
	scl->codec[1].type = GSM0808_SCT_FR2;

	scl->codec[2].fi = true;
	scl->codec[2].tf = true;
	scl->codec[2].type = GSM0808_SCT_CSD;
	scl->codec[2].cfg = 0xc0;

	scl->len = 3;
}

void test_gsm0808_enc_cause(void)
{
	/* NOTE: This must be tested early because many of the following tests
	 * rely on the generation of a proper cause code. */

	uint8_t rc_enc;
	struct msgb *msg;

	/* Test with a single byte cause code */
	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cause(msg, 0x41);
	EXPECT_ENCODED("04 01 41");
	msgb_free(msg);

	/* Test with an extended (two byte) cause code */
	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cause(msg, 0x8041);
	EXPECT_ENCODED("04 02 80 41");
	msgb_free(msg);
}

static void test_create_layer3(void)
{
	static const uint8_t res[] = {
		0x00, 0x0e, 0x57, 0x05, 0x08, 0x00, 0x77, 0x62,
		0x83, 0x33, 0x66, 0x44, 0x88, 0x17, 0x01, 0x23 };
	struct msgb *msg, *in_msg;
	struct osmo_cell_global_id cgi = {
		.lai = {
			.plmn = {
				.mcc = 0x2244,
				.mnc = 0x1122,
			},
			.lac = 0x3366,
		},
		.cell_identity = 0x4488,
	};
	printf("Testing creating Layer3\n");

	in_msg = msgb_alloc_headroom(512, 128, "foo");
	in_msg->l3h = in_msg->data;
	msgb_v_put(in_msg, 0x23);

	msg = gsm0808_create_layer3_2(in_msg, &cgi, NULL);
	VERIFY(msg, res, ARRAY_SIZE(res));
	msgb_free(msg);
	msgb_free(in_msg);
}

static void test_create_layer3_aoip()
{
	static const uint8_t res[] = {
		0x00, 0x17, 0x57, 0x05, 0x08, 0x00, 0x77, 0x62,
		0x83, 0x33, 0x66, 0x44, 0x88, 0x17, 0x01, 0x23,
		GSM0808_IE_SPEECH_CODEC_LIST, 0x07, GSM0808_SCT_FR3 | 0x50,
		0xef, 0xcd, GSM0808_SCT_FR2 | 0xa0, 0x9f,
		GSM0808_SCT_CSD | 0x90, 0xc0
	};
	struct osmo_cell_global_id cgi = {
		.lai = {
			.plmn = {
				.mcc = 0x2244,
				.mnc = 0x1122,
			},
			.lac = 0x3366,
		},
		.cell_identity = 0x4488,
	};
	struct msgb *msg, *in_msg;
	struct gsm0808_speech_codec_list sc_list;
	printf("Testing creating Layer3 (AoIP)\n");

	setup_codec_list(&sc_list);

	in_msg = msgb_alloc_headroom(512, 128, "foo");
	in_msg->l3h = in_msg->data;
	msgb_v_put(in_msg, 0x23);

	msg = gsm0808_create_layer3_2(in_msg, &cgi, &sc_list);

	VERIFY(msg, res, ARRAY_SIZE(res));

	msgb_free(msg);
	msgb_free(in_msg);
}

static void test_create_reset()
{
	static const uint8_t res[] = { 0x00, 0x04, 0x30, 0x04, 0x01, 0x20 };
	struct msgb *msg;

	printf("Testing creating Reset\n");
	msg = gsm0808_create_reset();
	VERIFY(msg, res, ARRAY_SIZE(res));
	msgb_free(msg);
}

static void test_create_reset_ack()
{
	static const uint8_t res[] = { 0x00, 0x01, 0x31 };
	struct msgb *msg;

	printf("Testing creating Reset Ack\n");
	msg = gsm0808_create_reset_ack();
	VERIFY(msg, res, ARRAY_SIZE(res));
	msgb_free(msg);
}


static void test_create_clear_command()
{
	static const uint8_t res[] = { 0x20, 0x04, 0x01, 0x23 };
	struct msgb *msg;

	printf("Testing creating Clear Command\n");
	msg = gsm0808_create_clear_command(0x23);
	VERIFY(msg, res, ARRAY_SIZE(res));
	msgb_free(msg);
}

static void test_create_clear_complete()
{
	static const uint8_t res[] = { 0x00, 0x01, 0x21 };
	struct msgb *msg;

	printf("Testing creating Clear Complete\n");
	msg = gsm0808_create_clear_complete();
	VERIFY(msg, res, ARRAY_SIZE(res));
	msgb_free(msg);
}

static void test_create_cipher()
{
	static const uint8_t res[] =
	    { 0x00, 0x0c, 0x53, 0x0a, 0x09, 0x03, 0xaa,
	      0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x23, 0x42 };
	static const uint8_t res2[] =
	    { 0x00, 0x0e, 0x53, 0x0a, 0x09, 0x03, 0xaa,
	      0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x23, 0x42,
	      GSM0808_IE_CIPHER_RESPONSE_MODE, 0x01 };
	struct msgb *msg;
	struct gsm0808_encrypt_info ei;
	uint8_t include_imeisv;

	memset(&ei, 0, sizeof(ei));
	ei.perm_algo[0] = GSM0808_ALG_ID_A5_0;
	ei.perm_algo[1] = GSM0808_ALG_ID_A5_1;
	ei.perm_algo_len = 2;
	ei.key[0] = 0xaa;
	ei.key[1] = 0xbb;
	ei.key[2] = 0xcc;
	ei.key[3] = 0xdd;
	ei.key[4] = 0xee;
	ei.key[5] = 0xff;
	ei.key[6] = 0x23;
	ei.key[7] = 0x42;
	ei.key_len = 8;
	include_imeisv = 1;

	printf("Testing creating Chipher Mode Command\n");
	msg = gsm0808_create_cipher(&ei, NULL);
	OSMO_ASSERT(msg);
	VERIFY(msg, res, ARRAY_SIZE(res));
	msgb_free(msg);

	msg = gsm0808_create_cipher(&ei, &include_imeisv);
	OSMO_ASSERT(msg);
	VERIFY(msg, res2, ARRAY_SIZE(res2));
	msgb_free(msg);
}

static void test_create_cipher_complete()
{
	static const uint8_t res1[] = {
		0x00, 0x08, 0x55, 0x20, 0x03, 0x23, 0x42, 0x21, 0x2c, 0x04 };
	static const uint8_t res2[] = { 0x00, 0x03, 0x55, 0x2c, 0x04};
	struct msgb *l3, *msg;

	printf("Testing creating Cipher Complete\n");
	l3 = msgb_alloc_headroom(512, 128, "l3h");
	l3->l3h = l3->data;
	msgb_v_put(l3, 0x23);
	msgb_v_put(l3, 0x42);
	msgb_v_put(l3, 0x21);

	/* with l3 data */
	msg = gsm0808_create_cipher_complete(l3, 4);
	VERIFY(msg, res1, ARRAY_SIZE(res1));
	msgb_free(msg);

	/* with l3 data but short */
	l3->len -= 1;
	l3->tail -= 1;
	msg = gsm0808_create_cipher_complete(l3, 4);
	VERIFY(msg, res2, ARRAY_SIZE(res2));
	msgb_free(msg);

	/* without l3 data */
	msg = gsm0808_create_cipher_complete(NULL, 4);
	VERIFY(msg, res2, ARRAY_SIZE(res2));
	msgb_free(msg);


	msgb_free(l3);
}

static inline void parse_cipher_reject(struct msgb *msg, uint8_t exp)
{
	struct tlv_parsed tp;
	int rc;

	/* skip header and message type so we can parse Cause IE directly */
	msg->l2h = msgb_data(msg) + sizeof(struct bssmap_header) + 1;

	rc = osmo_bssap_tlv_parse(&tp, msg->l2h, msgb_l2len(msg));
	if (rc < 0)
		printf("FIXME: failed (%d) to parse created message %s\n", rc, msgb_hexdump(msg));

	rc = gsm0808_get_cipher_reject_cause(&tp);
	if (rc < 0)
		printf("FIXME: failed (%s) to extract Cause from created message %s\n",
		       strerror(-rc), msgb_hexdump(msg));

	if (exp != (enum gsm0808_cause)rc)
		printf("FIXME: wrong Cause %d != %u (" OSMO_BIN_SPEC ") extracted from created message %s\n",
		       rc, exp, OSMO_BIT_PRINT(exp), msgb_hexdump(msg));
}

static void test_create_cipher_reject()
{
	static const uint8_t res[] = { 0x00, 0x04, 0x59, 0x04, 0x01, 0x23 };
	enum gsm0808_cause cause = GSM0808_CAUSE_CCCH_OVERLOAD;
	struct msgb *msg;

	printf("Testing creating Cipher Reject\n");
	msg = gsm0808_create_cipher_reject(cause);
	VERIFY(msg, res, ARRAY_SIZE(res));

	parse_cipher_reject(msg, cause);

	msgb_free(msg);
}

static void test_create_cipher_reject_ext()
{
	static const uint8_t res[] = { 0x00, 0x05, 0x59, 0x04, 0x02, 0xd0, 0xFA };
	uint8_t cause = 0xFA;
	struct msgb *msg;

	printf("Testing creating Cipher Reject (extended)\n");
	msg = gsm0808_create_cipher_reject_ext(GSM0808_CAUSE_CLASS_INVAL, cause);
	VERIFY(msg, res, ARRAY_SIZE(res));

	parse_cipher_reject(msg, cause);

	msgb_free(msg);
}

static void test_create_cm_u()
{
	static const uint8_t res[] = {
		0x00, 0x07, 0x54, 0x12, 0x01, 0x23, 0x13, 0x01, 0x42 };
	static const uint8_t res2o[] = {
		0x00, 0x04, 0x54, 0x12, 0x01, 0x23 };
	struct msgb *msg;
	const uint8_t cm2 = 0x23;
	const uint8_t cm3 = 0x42;

	printf("Testing creating CM U\n");
	msg = gsm0808_create_classmark_update(&cm2, 1, &cm3, 1);
	VERIFY(msg, res, ARRAY_SIZE(res));

	msgb_free(msg);

	msg = gsm0808_create_classmark_update(&cm2, 1, NULL, 0);
	VERIFY(msg, res2o, ARRAY_SIZE(res2o));

	msgb_free(msg);
}

static void test_create_sapi_reject()
{
	static const uint8_t res[] = { 0x00, 0x03, 0x25, 0x03, 0x25 };
	struct msgb *msg;

	printf("Testing creating SAPI Reject\n");
	msg = gsm0808_create_sapi_reject(3);
	VERIFY(msg, res, ARRAY_SIZE(res));
	msgb_free(msg);
}

static void test_create_ass()
{
	static const uint8_t res1[] =
	    { 0x00, 0x0a, 0x01, 0x0b, 0x04, 0x01, 0x0b, 0xa1, 0x25, 0x01, 0x00,
	      0x04 };
	static const uint8_t res2[] =
	    { 0x00, 0x20, 0x01, 0x0b, 0x04, 0x01, 0x0b, 0xa1, 0x25, 0x01, 0x00,
	      0x04, GSM0808_IE_AOIP_TRASP_ADDR, 0x06, 0xc0, 0xa8, 0x64, 0x17,
	      0x04, 0xd2, GSM0808_IE_SPEECH_CODEC_LIST, 0x07,
	      GSM0808_SCT_FR3 | 0x50, 0xef, 0xcd, GSM0808_SCT_FR2 | 0xa0, 0x9f,
	      GSM0808_SCT_CSD | 0x90, 0xc0, GSM0808_IE_CALL_ID, 0xaa, 0xbb,
	      0xcc, 0xdd };

	struct msgb *msg;
	struct gsm0808_channel_type ct;
	uint16_t cic = 0004;
	struct sockaddr_storage ss;
	struct sockaddr_in sin;
	struct gsm0808_speech_codec_list sc_list;
	uint32_t call_id = 0xAABBCCDD;

	memset(&ct, 0, sizeof(ct));
	ct.ch_indctr = GSM0808_CHAN_SPEECH;
	ct.ch_rate_type = GSM0808_SPEECH_HALF_PREF;
	ct.perm_spch[0] = GSM0808_PERM_FR3;
	ct.perm_spch[1] = GSM0808_PERM_HR3;
	ct.perm_spch_len = 2;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(1234);
	inet_aton("192.168.100.23", &sin.sin_addr);

	memset(&ss, 0, sizeof(ss));
	memcpy(&ss, &sin, sizeof(sin));

	setup_codec_list(&sc_list);

	printf("Testing creating Assignment Request\n");
	msg = gsm0808_create_ass(&ct, &cic, NULL, NULL, NULL);
	OSMO_ASSERT(msg);
	VERIFY(msg, res1, ARRAY_SIZE(res1));
	msgb_free(msg);

	msg = gsm0808_create_ass(&ct, &cic, &ss, &sc_list, &call_id);
	OSMO_ASSERT(msg);
	VERIFY(msg, res2, ARRAY_SIZE(res2));
	msgb_free(msg);
}

static void test_create_ass2()
{
	static const uint8_t res[] = {
		BSSAP_MSG_BSS_MANAGEMENT,
		0x45,
		BSS_MAP_MSG_ASSIGMENT_RQST,
		GSM0808_IE_CHANNEL_TYPE,
		0x04, 0x01, 0x0b, 0x91, 0x15, 0x01, 0x00, 0x04,
		GSM0808_IE_AOIP_TRASP_ADDR,
		0x06,
		0xac, 0x0c, 0x65, 0x0d, /* IPv4 */
		0x02, 0x9a,
		GSM0808_IE_SPEECH_CODEC_LIST,
		0x07,
		GSM0808_SCT_FR3 | 0x50,
		0xef, 0xcd,
		GSM0808_SCT_FR2 | 0xa0,
		0x9f,
		GSM0808_SCT_CSD | 0x90,
		0xc0,
		GSM0808_IE_CALL_ID,
		0xde, 0xad, 0xfa, 0xce, /* CallID */
		0x83, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, /* Kc */
		GSM0808_IE_GLOBAL_CALL_REF, 0x0d, /* GCR, length */
		0x03, 0x44, 0x44, 0x44, /* GCR, Net ID */
		0x02, 0xfe, 0xed, /* GCR, Node ID */
		0x05, 0x41, 0x41, 0x41, 0x41, 0x41, /* GCR, Call ref. ID */
		GSM0808_IE_LCLS_CONFIG, GSM0808_LCLS_CFG_BOTH_WAY,
		GSM0808_IE_LCLS_CONN_STATUS_CTRL, GSM0808_LCLS_CSC_CONNECT,
		GSM0808_IE_LCLS_CORR_NOT_NEEDED,
	};
	struct msgb *msg;
	struct gsm0808_channel_type ct;
	uint16_t cic = 4;
	struct sockaddr_storage ss;
	struct sockaddr_in sin;
	struct gsm0808_speech_codec_list sc_list;
	uint32_t call_id = 0xDEADFACE;
	struct osmo_gcr_parsed gcr = { .net_len = 3, .node = 0xFEED };
	uint8_t Kc[16];
	struct osmo_lcls lcls = {
		.config = GSM0808_LCLS_CFG_BOTH_WAY,
		.control = GSM0808_LCLS_CSC_CONNECT,
		.gcr = &gcr,
		.corr_needed = false
	};

	memset(gcr.cr, 'A', 5);
	memset(gcr.net, 'D', gcr.net_len);
	memset(Kc, 'E', 16);

	memset(&ct, 0, sizeof(ct));
	ct.ch_indctr = GSM0808_CHAN_SPEECH;
	ct.ch_rate_type = GSM0808_SPEECH_HALF_PREF;
	ct.perm_spch[0] = GSM0808_PERM_FR2;
	ct.perm_spch[1] = GSM0808_PERM_HR2;
	ct.perm_spch_len = 2;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(666);
	inet_aton("172.12.101.13", &sin.sin_addr); /* IPv4 */

	memset(&ss, 0, sizeof(ss));
	memcpy(&ss, &sin, sizeof(sin));

	setup_codec_list(&sc_list);

	printf("Testing creating Assignment Request with Kc and LCLS\n");

	msg = gsm0808_create_ass2(&ct, &cic, &ss, &sc_list, &call_id, Kc, &lcls);
	if (!msgb_eq_l3_data_print(msg, res, ARRAY_SIZE(res)))
		abort();

	msgb_free(msg);
}

static void test_create_ass_compl()
{
	static const uint8_t res1[] = {
		0x00, 0x09, 0x02, 0x15, 0x23, 0x21, 0x42, 0x2c,
		0x11, 0x40, 0x22 };
	static const uint8_t res2[] = {
		0x00, 0x07, 0x02, 0x15, 0x23, 0x21, 0x42, 0x2c, 0x11};
	struct msgb *msg;

	printf("Testing creating Assignment Complete\n");
	msg = gsm0808_create_assignment_completed(0x23, 0x42, 0x11, 0x22);
	VERIFY(msg, res1, ARRAY_SIZE(res1));
	msgb_free(msg);

	msg = gsm0808_create_assignment_completed(0x23, 0x42, 0x11, 0);
	VERIFY(msg, res2, ARRAY_SIZE(res2));
	msgb_free(msg);
}

static void test_create_ass_compl_aoip()
{
	struct sockaddr_storage ss;
	struct sockaddr_in sin;
	struct gsm0808_speech_codec sc;
	struct gsm0808_speech_codec_list sc_list;
	static const uint8_t res[] =
	    { 0x00, 0x1d, 0x02, 0x15, 0x23, 0x21, 0x42, 0x2c, 0x11, 0x40, 0x22,
	      GSM0808_IE_AOIP_TRASP_ADDR, 0x06, 0xc0, 0xa8, 0x64, 0x17, 0x04,
	      0xd2, GSM0808_IE_SPEECH_CODEC, 0x01, GSM0808_SCT_HR1 | 0x90,
	      GSM0808_IE_SPEECH_CODEC_LIST, 0x07, GSM0808_SCT_FR3 | 0x50, 0xef,
	      0xcd, GSM0808_SCT_FR2 | 0xa0, 0x9f, GSM0808_SCT_CSD | 0x90, 0xc0 };
	struct msgb *msg;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(1234);
	inet_aton("192.168.100.23", &sin.sin_addr);

	memset(&ss, 0, sizeof(ss));
	memcpy(&ss, &sin, sizeof(sin));

	memset(&sc, 0, sizeof(sc));
	sc.fi = true;
	sc.tf = true;
	sc.type = GSM0808_SCT_HR1;

	setup_codec_list(&sc_list);

	printf("Testing creating Assignment Complete (AoIP)\n");
	msg = gsm0808_create_ass_compl(0x23, 0x42, 0x11, 0x22,
				       &ss, &sc, &sc_list);
	VERIFY(msg, res, ARRAY_SIZE(res));
	msgb_free(msg);
}

static void test_create_ass_fail()
{
	static const uint8_t res1[] = { 0x00, 0x04, 0x03, 0x04, 0x01, 0x23 };
	static const uint8_t res2[] = {
		0x00, 0x06, 0x03, 0x04, 0x01, 0x23, 0x15, 0x02};
	uint8_t rr_res = 2;
	struct msgb *msg;

	printf("Testing creating Assignment Failure\n");
	msg = gsm0808_create_assignment_failure(0x23, NULL);
	VERIFY(msg, res1, ARRAY_SIZE(res1));
	msgb_free(msg);

	msg = gsm0808_create_assignment_failure(0x23, &rr_res);
	VERIFY(msg, res2, ARRAY_SIZE(res2));
	msgb_free(msg);
}

static void test_create_ass_fail_aoip()
{
	static const uint8_t res1[] =
	    { 0x00, 0x0d, 0x03, 0x04, 0x01, 0x23, GSM0808_IE_SPEECH_CODEC_LIST,
	      0x07, GSM0808_SCT_FR3 | 0x50, 0xef, 0xcd, GSM0808_SCT_FR2 | 0xa0,
	      0x9f, GSM0808_SCT_CSD | 0x90, 0xc0 };
	static const uint8_t res2[] =
	    { 0x00, 0x0f, 0x03, 0x04, 0x01, 0x23, 0x15, 0x02,
	      GSM0808_IE_SPEECH_CODEC_LIST, 0x07, GSM0808_SCT_FR3 | 0x50, 0xef,
	      0xcd, GSM0808_SCT_FR2 | 0xa0, 0x9f, GSM0808_SCT_CSD | 0x90, 0xc0 };
	uint8_t rr_res = 2;
	struct msgb *msg;
	struct gsm0808_speech_codec_list sc_list;

	setup_codec_list(&sc_list);

	printf("Testing creating Assignment Failure (AoIP)\n");
	msg = gsm0808_create_ass_fail(0x23, NULL, &sc_list);
	VERIFY(msg, res1, ARRAY_SIZE(res1));
	msgb_free(msg);

	msg = gsm0808_create_ass_fail(0x23, &rr_res, &sc_list);
	VERIFY(msg, res2, ARRAY_SIZE(res2));
	msgb_free(msg);
}

static void test_create_clear_rqst()
{
	static const uint8_t res[] = { 0x00, 0x04, 0x22, 0x04, 0x01, 0x23 };
	struct msgb *msg;

	printf("Testing creating Clear Request\n");
	msg = gsm0808_create_clear_rqst(0x23);
	VERIFY(msg, res, ARRAY_SIZE(res));
	msgb_free(msg);
}

static void test_create_paging()
{
	static const uint8_t res[] =
	    { 0x00, 0x10, 0x52, 0x08, 0x08, 0x09, 0x10, 0x10, 0x00, 0x00, 0x00,
	      0x21, 0x43, 0x1a, 0x03, 0x05, 0x23, 0x42 };
	static const uint8_t res2[] =
	    { 0x00, 0x16, 0x52, 0x08, 0x08, 0x09, 0x10, 0x10, 0x00, 0x00, 0x00,
	      0x21, 0x43, GSM0808_IE_TMSI, 0x04, 0x12, 0x34, 0x56, 0x78, 0x1a,
	      0x03, 0x05, 0x23, 0x42 };
	static const uint8_t res3[] =
	    { 0x00, 0x18, 0x52, 0x08, 0x08, 0x09, 0x10, 0x10, 0x00, 0x00, 0x00,
	      0x21, 0x43, GSM0808_IE_TMSI, 0x04, 0x12, 0x34, 0x56, 0x78, 0x1a,
	      0x03, 0x05, 0x23, 0x42, GSM0808_IE_CHANNEL_NEEDED,
	      RSL_CHANNEED_TCH_ForH };

	struct msgb *msg;
	struct gsm0808_cell_id_list2 cil;
	uint32_t tmsi = 0x12345678;
	uint8_t chan_needed = RSL_CHANNEED_TCH_ForH;

	char imsi[] = "001010000001234";

	cil.id_discr = CELL_IDENT_LAC;
	cil.id_list[0].lac = 0x2342;
	cil.id_list_len = 1;

	printf("Testing creating Paging Request\n");
	msg = gsm0808_create_paging2(imsi, NULL, &cil, NULL);
	VERIFY(msg, res, ARRAY_SIZE(res));
	msgb_free(msg);

	msg = gsm0808_create_paging2(imsi, &tmsi, &cil, NULL);
	VERIFY(msg, res2, ARRAY_SIZE(res2));
	msgb_free(msg);

	msg = gsm0808_create_paging2(imsi, &tmsi, &cil, &chan_needed);
	VERIFY(msg, res3, ARRAY_SIZE(res3));
	msgb_free(msg);
}

static void test_create_dtap()
{
	static const uint8_t res[] = { 0x01, 0x03, 0x02, 0x23, 0x42 };
	struct msgb *msg, *l3;

	printf("Testing creating DTAP\n");
	l3 = msgb_alloc_headroom(512, 128, "test");
	l3->l3h = l3->data;
	msgb_v_put(l3, 0x23);
	msgb_v_put(l3, 0x42);

	msg = gsm0808_create_dtap(l3, 0x3);
	VERIFY(msg, res, ARRAY_SIZE(res));
	msgb_free(msg);
	msgb_free(l3);
}

static void test_prepend_dtap()
{
	static const uint8_t res[] = { 0x01, 0x03, 0x02, 0x23, 0x42 };
	struct msgb *in_msg;

	printf("Testing prepend DTAP\n");

	in_msg = msgb_alloc_headroom(512, 128, "test");
	msgb_v_put(in_msg, 0x23);
	msgb_v_put(in_msg, 0x42);

	gsm0808_prepend_dtap_header(in_msg, 0x3);
	in_msg->l3h = in_msg->data;
	VERIFY(in_msg, res, ARRAY_SIZE(res));
	msgb_free(in_msg);
}

static void test_enc_dec_lcls()
{
	static const uint8_t res[] = {
		GSM0808_IE_GLOBAL_CALL_REF,
		0x0d, /* GCR length */
		0x03, /* .net_len */
		0xf1, 0xf2, 0xf3, /* .net */
		0x02, /* .node length */
		0xde, 0xad, /* .node */
		0x05, /* length of Call. Ref. */
		0x41, 0x42, 0x43, 0x44, 0x45 /* .cr - Call. Ref. */
	};
	uint8_t len;
	struct msgb *msg;
	struct osmo_gcr_parsed p = { 0 }, g = {
		.net_len = 3,
		.net = { 0xf1, 0xf2, 0xf3 },
		.node = 0xDEAD,
		.cr = { 0x41, 0x42, 0x43, 0x44, 0x45 },
	};
	int rc;
	struct tlv_parsed tp;
	struct osmo_lcls lcls_out = { .gcr = &p }, lcls_in = {
		.gcr = &g,
		.config = GSM0808_LCLS_CFG_NA,
		.control = GSM0808_LCLS_CSC_NA,
		.corr_needed = true,
	};

	msg = msgb_alloc_headroom(BSSMAP_MSG_SIZE, BSSMAP_MSG_HEADROOM, "LCLS IE");
	if (!msg)
		return;

	len = gsm0808_enc_lcls(msg, &lcls_in);
	printf("Testing Global Call Reference IE encoder...\n\t%d bytes added: %s\n",
	       len, len == ARRAY_SIZE(res) ? "OK" : "FAIL");

	if (!msgb_eq_data_print(msg, res, ARRAY_SIZE(res)))
		abort();

	rc = osmo_bssap_tlv_parse(&tp, msgb_data(msg), msgb_length(msg));
	if (rc < 0) {
		printf("parsing failed: %s [%s]\n", strerror(-rc), msgb_hexdump(msg));
		abort();
	}

	rc = gsm0808_dec_lcls(&lcls_out, &tp);
	if (rc < 0) {
		printf("decoding failed: %s [%s]\n", strerror(-rc), msgb_hexdump(msg));
		abort();
	}

	if (lcls_out.gcr->net_len != g.net_len) {
		printf("Network ID length parsed wrong: %u != %u\n", lcls_out.gcr->net_len, g.net_len);
		abort();
	}

	if (lcls_out.gcr->node != g.node) {
		printf("Node ID parsed wrong: 0x%X != 0x%X\n", lcls_out.gcr->node, g.node);
		abort();
	}

	if (memcmp(lcls_out.gcr->net, g.net, g.net_len) != 0) {
		printf("Network ID parsed wrong: %s\n", osmo_hexdump(lcls_out.gcr->net, lcls_out.gcr->net_len));
		abort();
	}

	if (memcmp(lcls_out.gcr->cr, g.cr, 5) != 0) {
		printf("Call ref. ID parsed wrong: %s\n", osmo_hexdump(lcls_out.gcr->cr, 5));
		abort();
	}

	printf("\tdecoded %d bytes: %s\n", rc, rc == len ? "OK" : "FAIL");
	msgb_free(msg);
}

static void test_enc_dec_aoip_trasp_addr_v4()
{
	struct sockaddr_storage enc_addr;
	struct sockaddr_storage dec_addr;
	struct sockaddr_in enc_addr_in;
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	memset(&enc_addr_in, 0, sizeof(enc_addr_in));
	enc_addr_in.sin_family = AF_INET;
	enc_addr_in.sin_port = htons(1234);
	inet_aton("255.0.255.255", &enc_addr_in.sin_addr);

	memset(&enc_addr, 0, sizeof(enc_addr));
	memcpy(&enc_addr, &enc_addr_in, sizeof(enc_addr_in));

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_aoip_trasp_addr(msg, &enc_addr);
	OSMO_ASSERT(rc_enc == 8);
	rc_dec =
	    gsm0808_dec_aoip_trasp_addr(&dec_addr, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 6);
	OSMO_ASSERT(memcmp(&enc_addr, &dec_addr, sizeof(enc_addr)) == 0);

	msgb_free(msg);
}

static void test_enc_dec_aoip_trasp_addr_v6()
{
	struct sockaddr_storage enc_addr;
	struct sockaddr_storage dec_addr;
	struct sockaddr_in6 enc_addr_in;
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	memset(&enc_addr_in, 0, sizeof(enc_addr_in));
	enc_addr_in.sin6_family = AF_INET6;
	enc_addr_in.sin6_port = htons(4567);
	inet_pton(AF_INET6, "2001:0db8:85a3:08d3:1319:8a2e:0370:7344",
		  &enc_addr_in.sin6_addr);

	memset(&enc_addr, 0, sizeof(enc_addr));
	memcpy(&enc_addr, &enc_addr_in, sizeof(enc_addr_in));

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_aoip_trasp_addr(msg, &enc_addr);
	OSMO_ASSERT(rc_enc == 20);
	rc_dec =
	    gsm0808_dec_aoip_trasp_addr(&dec_addr, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 18);
	OSMO_ASSERT(memcmp(&enc_addr, &dec_addr, sizeof(enc_addr)) == 0);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_speech_codec()
{
	struct gsm0808_speech_codec enc_sc = {
		.pi = true,
		.tf = true,
		.type = GSM0808_SCT_FR2,
	};
	struct gsm0808_speech_codec dec_sc = {};
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_speech_codec(msg, &enc_sc);
	OSMO_ASSERT(rc_enc == 3);

	rc_dec = gsm0808_dec_speech_codec(&dec_sc, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 1);

	OSMO_ASSERT(memcmp(&enc_sc, &dec_sc, sizeof(enc_sc)) == 0);

	msgb_free(msg);
}


static void test_gsm0808_enc_dec_speech_codec_with_cfg()
{
	struct gsm0808_speech_codec enc_sc = {
		.pi = true,
		.tf = true,
		.type = GSM0808_SCT_FR3,
		.cfg = 0xabcd,
	};
	struct gsm0808_speech_codec dec_sc = {};
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_speech_codec(msg, &enc_sc);
	OSMO_ASSERT(rc_enc == 5);

	rc_dec = gsm0808_dec_speech_codec(&dec_sc, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 3);

	OSMO_ASSERT(memcmp(&enc_sc, &dec_sc, sizeof(enc_sc)) == 0);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_speech_codec_ext_with_cfg()
{
	struct gsm0808_speech_codec enc_sc = {
		.pi = true,
		.tf = true,
		.type = GSM0808_SCT_CSD,
		.cfg = 0xc0,
	};
	struct gsm0808_speech_codec dec_sc = {};
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_speech_codec(msg, &enc_sc);
	OSMO_ASSERT(rc_enc == 5);

	rc_dec = gsm0808_dec_speech_codec(&dec_sc, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 3);

	OSMO_ASSERT(memcmp(&enc_sc, &dec_sc, sizeof(enc_sc)) == 0);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_speech_codec_list()
{
	struct gsm0808_speech_codec_list enc_scl = {
		.codec = {
			{
				.pi = true,
				.tf = true,
				.type = GSM0808_SCT_FR3,
				.cfg = 0xcdef,
			},

			{
				.fi = true,
				.pt = true,
				.type = GSM0808_SCT_FR2,
			},

			{
				.fi = true,
				.tf = true,
				.type = GSM0808_SCT_CSD,
				.cfg = 0xc0,
			},
		},
		.len = 3,
	};
	struct gsm0808_speech_codec_list dec_scl = {};
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_speech_codec_list(msg, &enc_scl);
	OSMO_ASSERT(rc_enc == 9);

	rc_dec = gsm0808_dec_speech_codec_list(&dec_scl, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 7);

	OSMO_ASSERT(memcmp(&enc_scl, &dec_scl, sizeof(enc_scl)) == 0);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_empty_speech_codec_list()
{
	struct gsm0808_speech_codec_list enc_scl = {
		.len = 0,
	};
	struct gsm0808_speech_codec_list dec_scl = {};
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_speech_codec_list(msg, &enc_scl);
	OSMO_ASSERT(rc_enc == 2);

	rc_dec = gsm0808_dec_speech_codec_list(&dec_scl, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 0);

	OSMO_ASSERT(memcmp(&enc_scl, &dec_scl, sizeof(enc_scl)) == 0);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_channel_type()
{
	struct gsm0808_channel_type enc_ct = {
		.ch_indctr = GSM0808_CHAN_SPEECH,
		.ch_rate_type = GSM0808_SPEECH_HALF_PREF,
		.perm_spch = { GSM0808_PERM_FR3, GSM0808_PERM_HR3 },
		.perm_spch_len = 2,
	};
	struct gsm0808_channel_type dec_ct = {};
	struct msgb *msg;
	uint8_t ct_enc_expected[] = { GSM0808_IE_CHANNEL_TYPE,
		0x04, 0x01, 0x0b, 0xa1, 0x25
	};
	uint8_t rc_enc;
	int rc_dec;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_channel_type(msg, &enc_ct);
	OSMO_ASSERT(rc_enc == 6);
	OSMO_ASSERT(memcmp(ct_enc_expected, msg->data, msg->len) == 0);

	rc_dec = gsm0808_dec_channel_type(&dec_ct, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 4);
	OSMO_ASSERT(memcmp(&enc_ct, &dec_ct, sizeof(enc_ct)) == 0);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_encrypt_info()
{
	struct gsm0808_encrypt_info enc_ei = {
		.perm_algo = { GSM0808_ALG_ID_A5_0, GSM0808_ALG_ID_A5_1 },
		.perm_algo_len = 2,
		.key = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x23, 0x42, },
		.key_len = 8,
	};
	struct gsm0808_encrypt_info dec_ei = {};
	struct msgb *msg;
	uint8_t ei_enc_expected[] =
	    { GSM0808_IE_ENCRYPTION_INFORMATION, 0x09, 0x03, 0xaa, 0xbb,
		0xcc, 0xdd, 0xee, 0xff, 0x23, 0x42
	};
	uint8_t rc_enc;
	int rc_dec;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_encrypt_info(msg, &enc_ei);
	OSMO_ASSERT(rc_enc == 11);
	OSMO_ASSERT(memcmp(ei_enc_expected, msg->data, msg->len) == 0);

	rc_dec = gsm0808_dec_encrypt_info(&dec_ei, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 9);

	OSMO_ASSERT(memcmp(&enc_ei, &dec_ei, sizeof(enc_ei)) == 0);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_list_lac()
{
	struct gsm0808_cell_id_list2 enc_cil;
	struct gsm0808_cell_id_list2 dec_cil;
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	memset(&enc_cil, 0, sizeof(enc_cil));
	enc_cil.id_discr = CELL_IDENT_LAC;
	enc_cil.id_list[0].lac = 0x0124;
	enc_cil.id_list[1].lac = 0xABCD;
	enc_cil.id_list[2].lac = 0x5678;
	enc_cil.id_list_len = 3;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id_list2(msg, &enc_cil);
	EXPECT_ENCODED("1a 07 05 01 24 ab cd 56 78");

	rc_dec = gsm0808_dec_cell_id_list2(&dec_cil, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 7);

	OSMO_ASSERT(memcmp(&enc_cil, &dec_cil, sizeof(enc_cil)) == 0);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_list_single_lac()
{
	struct gsm0808_cell_id_list2 enc_cil;
	struct gsm0808_cell_id_list2 dec_cil;
	struct msgb *msg;
	uint8_t cil_enc_expected[] = { GSM0808_IE_CELL_IDENTIFIER_LIST, 0x03,
		0x05, 0x23, 0x42
	};
	uint8_t rc_enc;
	int rc_dec;

	memset(&enc_cil, 0, sizeof(enc_cil));
	enc_cil.id_discr = CELL_IDENT_LAC;
	enc_cil.id_list[0].lac = 0x2342;
	enc_cil.id_list_len = 1;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id_list2(msg, &enc_cil);
	OSMO_ASSERT(rc_enc == 5);
	OSMO_ASSERT(memcmp(cil_enc_expected, msg->data, msg->len) == 0);

	rc_dec = gsm0808_dec_cell_id_list2(&dec_cil, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 3);

	OSMO_ASSERT(memcmp(&enc_cil, &dec_cil, sizeof(enc_cil)) == 0);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_list_multi_lac()
{
	struct gsm0808_cell_id_list2 enc_cil;
	struct gsm0808_cell_id_list2 dec_cil;
	struct msgb *msg;
	uint8_t cil_enc_expected[] = { GSM0808_IE_CELL_IDENTIFIER_LIST, 0x0b, 0x05,
		0x23, 0x42,
		0x24, 0x43,
		0x25, 0x44,
		0x26, 0x45,
		0x27, 0x46
	};
	uint8_t rc_enc;
	int rc_dec;

	memset(&enc_cil, 0, sizeof(enc_cil));
	enc_cil.id_discr = CELL_IDENT_LAC;
	enc_cil.id_list[0].lac = 0x2342;
	enc_cil.id_list[1].lac = 0x2443;
	enc_cil.id_list[2].lac = 0x2544;
	enc_cil.id_list[3].lac = 0x2645;
	enc_cil.id_list[4].lac = 0x2746;
	enc_cil.id_list_len = 5;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id_list2(msg, &enc_cil);
	OSMO_ASSERT(rc_enc == sizeof(cil_enc_expected));
	OSMO_ASSERT(memcmp(cil_enc_expected, msg->data, msg->len) == 0);

	rc_dec = gsm0808_dec_cell_id_list2(&dec_cil, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == msg->len - 2);
	OSMO_ASSERT(memcmp(&enc_cil, &dec_cil, sizeof(enc_cil)) == 0);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_list_bss()
{
	struct gsm0808_cell_id_list2 enc_cil;
	struct gsm0808_cell_id_list2 dec_cil;
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	memset(&enc_cil, 0, sizeof(enc_cil));
	enc_cil.id_discr = CELL_IDENT_BSS;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id_list2(msg, &enc_cil);
	OSMO_ASSERT(rc_enc == 3);

	rc_dec = gsm0808_dec_cell_id_list2(&dec_cil, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 1);

	OSMO_ASSERT(memcmp(&enc_cil, &dec_cil, sizeof(enc_cil)) == 0);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_list_multi_lai_and_lac()
{
	struct gsm0808_cell_id_list2 enc_cil;
	struct gsm0808_cell_id_list2 dec_cil;
	struct osmo_location_area_id id;
	struct msgb *msg;
	uint8_t cil_enc_expected[] = { GSM0808_IE_CELL_IDENTIFIER_LIST, 0x10, 0x04,
		0x92, 0x61, 0x54, 0x23, 0x42,
		0x92, 0x72, 0x54, 0x24, 0x43,
		0x92, 0x83, 0x54, 0x25, 0x44
	};
	uint8_t rc_enc;
	int rc_dec, i;

	memset(&enc_cil, 0, sizeof(enc_cil));
	enc_cil.id_discr = CELL_IDENT_LAI_AND_LAC;

	id.plmn.mcc = 0x123;
	osmo_mnc_from_str("456", &id.plmn.mnc, &id.plmn.mnc_3_digits);
	id.lac = 0x2342;
	memcpy(&enc_cil.id_list[0].lai_and_lac, &id, sizeof(id));

	id.plmn.mcc = 0x124;
	osmo_mnc_from_str("457", &id.plmn.mnc, &id.plmn.mnc_3_digits);
	id.lac = 0x2443;
	memcpy(&enc_cil.id_list[1].lai_and_lac, &id, sizeof(id));

	id.plmn.mcc = 0x125;
	osmo_mnc_from_str("458", &id.plmn.mnc, &id.plmn.mnc_3_digits);
	id.lac = 0x2544;
	memcpy(&enc_cil.id_list[2].lai_and_lac, &id, sizeof(id));

	enc_cil.id_list_len = 3;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id_list2(msg, &enc_cil);
	OSMO_ASSERT(rc_enc == sizeof(cil_enc_expected));
	OSMO_ASSERT(memcmp(cil_enc_expected, msg->data, msg->len) == 0);

	rc_dec = gsm0808_dec_cell_id_list2(&dec_cil, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == msg->len - 2);

	OSMO_ASSERT(dec_cil.id_list_len == 3);
	/* Check MAXLEN elements to ensure everything has been initialized. */
	for (i = 0; i < GSM0808_CELL_ID_LIST2_MAXLEN; i++) {
		struct osmo_location_area_id *enc_id;
		struct osmo_location_area_id *dec_id;
		enc_id = &enc_cil.id_list[i].lai_and_lac;
		dec_id = &dec_cil.id_list[i].lai_and_lac;
		OSMO_ASSERT(osmo_plmn_cmp(&enc_id->plmn, &dec_id->plmn) == 0);
		OSMO_ASSERT(enc_id->lac == dec_id->lac);
	}

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_list_multi_ci()
{
	struct gsm0808_cell_id_list2 enc_cil;
	struct gsm0808_cell_id_list2 dec_cil;
	struct msgb *msg;
	uint8_t cil_enc_expected[] = { GSM0808_IE_CELL_IDENTIFIER_LIST, 0x09, 0x02,
		0x00, 0x01,
		0x00, 0x02,
		0x00, 0x77,
		0x01, 0xff,
	};
	uint8_t rc_enc;
	int rc_dec;

	memset(&enc_cil, 0, sizeof(enc_cil));
	enc_cil.id_discr = CELL_IDENT_CI;
	enc_cil.id_list[0].ci = 1;
	enc_cil.id_list[1].ci = 2;
	enc_cil.id_list[2].ci = 119;
	enc_cil.id_list[3].ci = 511;
	enc_cil.id_list_len = 4;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id_list2(msg, &enc_cil);
	OSMO_ASSERT(rc_enc == sizeof(cil_enc_expected));
	OSMO_ASSERT(memcmp(cil_enc_expected, msg->data, msg->len) == 0);

	rc_dec = gsm0808_dec_cell_id_list2(&dec_cil, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == msg->len - 2);
	OSMO_ASSERT(memcmp(&enc_cil, &dec_cil, sizeof(enc_cil)) == 0);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_list_multi_lac_and_ci()
{
	struct gsm0808_cell_id_list2 enc_cil;
	struct gsm0808_cell_id_list2 dec_cil;
	struct msgb *msg;
	uint8_t cil_enc_expected[] = { GSM0808_IE_CELL_IDENTIFIER_LIST, 0x15, 0x01,
		0x23, 0x42, 0x00, 0x01,
		0x24, 0x43, 0x00, 0x02,
		0x25, 0x44, 0x00, 0x77,
		0x26, 0x45, 0x01, 0xff,
		0x27, 0x46, 0x02, 0xfe,
	};
	uint8_t rc_enc;
	int rc_dec;

	memset(&enc_cil, 0, sizeof(enc_cil));
	enc_cil.id_discr = CELL_IDENT_LAC_AND_CI;
	enc_cil.id_list[0].lac_and_ci.lac = 0x2342;
	enc_cil.id_list[0].lac_and_ci.ci = 1;
	enc_cil.id_list[1].lac_and_ci.lac = 0x2443;
	enc_cil.id_list[1].lac_and_ci.ci = 2;
	enc_cil.id_list[2].lac_and_ci.lac = 0x2544;
	enc_cil.id_list[2].lac_and_ci.ci = 119;
	enc_cil.id_list[3].lac_and_ci.lac = 0x2645;
	enc_cil.id_list[3].lac_and_ci.ci = 511;
	enc_cil.id_list[4].lac_and_ci.lac = 0x2746;
	enc_cil.id_list[4].lac_and_ci.ci = 766;
	enc_cil.id_list_len = 5;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id_list2(msg, &enc_cil);
	OSMO_ASSERT(rc_enc == sizeof(cil_enc_expected));
	OSMO_ASSERT(memcmp(cil_enc_expected, msg->data, msg->len) == 0);

	rc_dec = gsm0808_dec_cell_id_list2(&dec_cil, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == msg->len - 2);
	OSMO_ASSERT(memcmp(&enc_cil, &dec_cil, sizeof(enc_cil)) == 0);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_list_multi_global()
{
	struct gsm0808_cell_id_list2 enc_cil;
	struct gsm0808_cell_id_list2 dec_cil;
	struct msgb *msg;
	uint8_t cil_enc_expected[] = { GSM0808_IE_CELL_IDENTIFIER_LIST, 0x16, 0x00,
		0x21,  0x63,  0x54,  0x23,  0x42,  0x00,  0x1,
		0x21,  0xf4,  0x75,  0x24,  0x43,  0x00,  0x2,
		0x21,  0x75,  0x00,  0x25,  0x44,  0x00,  0x77
	};
	uint8_t rc_enc;
	int rc_dec, i;

	enc_cil = (struct gsm0808_cell_id_list2){
		.id_discr = CELL_IDENT_WHOLE_GLOBAL,
		.id_list_len = 3,
		.id_list = {
			{
				.global = {
					.lai = { .plmn = { .mcc = 123, .mnc = 456 },
						 .lac = 0x2342 },
					.cell_identity = 1,
				}
			},
			{
				.global = {
					.lai = { .plmn = { .mcc = 124, .mnc = 57 },
						 .lac = 0x2443 },
					.cell_identity = 2,
				}
			},
			{
				.global = {
					.lai = { .plmn = { .mcc = 125, .mnc = 7,
						           .mnc_3_digits = true },
						 .lac = 0x2544 },
					.cell_identity = 119,
				}
			},
		}
	};

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id_list2(msg, &enc_cil);
	OSMO_ASSERT(rc_enc == sizeof(cil_enc_expected));
	if (memcmp(cil_enc_expected, msg->data, msg->len)) {
		printf("   got: %s\n", osmo_hexdump(msg->data, msg->len));
		printf("expect: %s\n", osmo_hexdump(cil_enc_expected, sizeof(cil_enc_expected)));
		OSMO_ASSERT(false);
	}

	rc_dec = gsm0808_dec_cell_id_list2(&dec_cil, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == msg->len - 2);

	/* Check MAXLEN elements to ensure everything has been initialized. */
	for (i = 0; i < GSM0808_CELL_ID_LIST2_MAXLEN; i++) {
		struct osmo_cell_global_id *enc_id;
		struct osmo_cell_global_id *dec_id;
		enc_id = &enc_cil.id_list[i].global;
		dec_id = &dec_cil.id_list[i].global;
		OSMO_ASSERT(osmo_plmn_cmp(&enc_id->lai.plmn, &dec_id->lai.plmn) == 0);
		OSMO_ASSERT(enc_id->lai.lac == dec_id->lai.lac);
		OSMO_ASSERT(enc_id->cell_identity == dec_id->cell_identity);
	}

	msgb_free(msg);
}

static void print_cil(const struct gsm0808_cell_id_list2 *cil)
{
	printf("     cell_id_list == %s\n", gsm0808_cell_id_list_name(cil));
}

void test_cell_id_list_add() {
	size_t zu;

	const struct gsm0808_cell_id_list2 cgi1 = {
		.id_discr = CELL_IDENT_WHOLE_GLOBAL,
		.id_list_len = 1,
		.id_list = {
			{
				.global = {
					.lai = {
						.plmn = { .mcc = 1, .mnc = 2, .mnc_3_digits = false },
						.lac = 3,
					},
					.cell_identity = 4,
				}
			},
		},
	};

	const struct gsm0808_cell_id_list2 cgi2 = {
		.id_discr = CELL_IDENT_WHOLE_GLOBAL,
		.id_list_len = 2,
		.id_list = {
			{
				.global = {
					.lai = {
						.plmn = { .mcc = 1, .mnc = 2, .mnc_3_digits = true },
						.lac = 3,
					},
					.cell_identity = 4,
				}
			},
			{
				.global = {
					.lai = {
						.plmn = { .mcc = 5, .mnc = 6, .mnc_3_digits = true },
						.lac = 7,
					},
					.cell_identity = 8,
				}
			},
		},
	};

	const struct gsm0808_cell_id_list2 cgi2a = {
		.id_discr = CELL_IDENT_WHOLE_GLOBAL,
		.id_list_len = 2,
		.id_list = {
			{
				.global = cgi2.id_list[0].global
			},
			{
				.global = {
					.lai = {
						.plmn = { .mcc = 9, .mnc = 10, .mnc_3_digits = true },
						.lac = 11,
					},
					.cell_identity = 12,
				}
			},
		},
	};

	const struct gsm0808_cell_id_list2 cgi3 = {
		.id_discr = CELL_IDENT_WHOLE_GLOBAL,
		.id_list_len = 2,
		.id_list = {
			{
				.global = {
					.lai = {
						.plmn = { .mcc = 13, .mnc = 14, .mnc_3_digits = true },
						.lac = 15,
					},
					.cell_identity = 16,
				}
			},
			{
				.global = {
					.lai = {
						.plmn = { .mcc = 16, .mnc = 17, .mnc_3_digits = true },
						.lac = 18,
					},
					.cell_identity = 19,
				}
			},
		},
	};


	const struct gsm0808_cell_id_list2 lac1 = {
		.id_discr = CELL_IDENT_LAC,
		.id_list_len = 1,
		.id_list = {
			{
				.lac = 123
			},
		},
	};

	const struct gsm0808_cell_id_list2 lac2 = {
		.id_discr = CELL_IDENT_LAC,
		.id_list_len = 2,
		.id_list = {
			{
				.lac = 456
			},
			{
				.lac = 789
			},
		},
	};

	struct gsm0808_cell_id_list2 cil = {};

	printf("------- %s\n", __func__);

	print_cil(&cil);

#define ADD_QUIET(other_cil, expect_rc) do { \
		int rc = gsm0808_cell_id_list_add(&cil, &other_cil); \
		printf("gsm0808_cell_id_list_add(&cil, &" #other_cil ") --> rc = %d\n", rc); \
		OSMO_ASSERT(rc == expect_rc); \
	} while(0)

#define ADD(other_cil, expect_rc) ADD_QUIET(other_cil, expect_rc); print_cil(&cil)

	ADD(lac1, 1);
	ADD(lac1, 0);
	ADD(lac2, 2);
	ADD(lac2, 0);
	ADD(cil, 0);
	ADD(cgi1, -EINVAL);

	printf("* can't add to BSS list\n");
	cil.id_list_len = 0;
	cil.id_discr = CELL_IDENT_BSS;
	print_cil(&cil);
	ADD(lac1, -EINVAL);

	printf("* other types (including NO_CELL) take on new type iff empty\n");
	cil.id_list_len = 0;
	cil.id_discr = CELL_IDENT_NO_CELL;
	print_cil(&cil);
	ADD(cgi1, 1);
	ADD(cgi1, 0);
	ADD(cgi2, 2);
	ADD(cgi2, 0);

	printf("* test gsm0808_cell_id_list_name_buf()'s return val\n");
	zu = strlen(gsm0808_cell_id_list_name(&cil));
	printf("  strlen(gsm0808_cell_id_list_name(cil)) == %zu\n", zu);
	zu ++;
	while (1) {
		char buf[128] = "?";
		int rc;
		OSMO_ASSERT(zu < sizeof(buf));
		buf[zu] = '#';
		rc = gsm0808_cell_id_list_name_buf(buf, zu, &cil);
		printf("  gsm0808_cell_id_list_name_buf(buf, %zu, cil)) == %d \"%s\"\n",
		       zu, rc, buf);
		OSMO_ASSERT(buf[zu] == '#');
		if (!zu)
			break;
		zu /= 2;
	}

	printf("* list-full behavior\n");
	cil.id_list_len = GSM0808_CELL_ID_LIST2_MAXLEN - 1;
	printf("cil.id_list_len = %u\n", cil.id_list_len);
	ADD_QUIET(cgi2a, 1);
	printf("cil.id_list_len = %u\n", cil.id_list_len);

	cil.id_list_len = GSM0808_CELL_ID_LIST2_MAXLEN - 1;
	printf("cil.id_list_len = %u\n", cil.id_list_len);
	ADD_QUIET(cgi3, -ENOSPC);
	printf("cil.id_list_len = %u\n", cil.id_list_len);
	ADD_QUIET(cgi2a, -ENOSPC);
	printf("cil.id_list_len = %u\n", cil.id_list_len);

	printf("------- %s done\n", __func__);
}

static void test_gsm0808_enc_dec_cell_id_lac()
{
	struct gsm0808_cell_id enc_ci = {
		.id_discr = CELL_IDENT_LAC,
		.id.lac = 0x0124,
	};
	struct gsm0808_cell_id dec_ci;
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	memset(&dec_ci, 0xa5, sizeof(dec_ci));

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id(msg, &enc_ci);
	EXPECT_ENCODED("05 03 05 01 24");

	rc_dec = gsm0808_dec_cell_id(&dec_ci, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 3);

	OSMO_ASSERT(enc_ci.id_discr == dec_ci.id_discr
		    && enc_ci.id.lac == dec_ci.id.lac);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_bss()
{
	struct gsm0808_cell_id enc_ci = {
		.id_discr = CELL_IDENT_BSS,
	};
	struct gsm0808_cell_id dec_ci;
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id(msg, &enc_ci);
	EXPECT_ENCODED("05 01 06");

	rc_dec = gsm0808_dec_cell_id(&dec_ci, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 1);

	OSMO_ASSERT(enc_ci.id_discr == dec_ci.id_discr);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_no_cell()
{
	struct gsm0808_cell_id enc_ci = {
		.id_discr = CELL_IDENT_NO_CELL,
	};
	struct gsm0808_cell_id dec_ci;
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id(msg, &enc_ci);
	EXPECT_ENCODED("05 01 03");

	rc_dec = gsm0808_dec_cell_id(&dec_ci, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == 1);

	OSMO_ASSERT(enc_ci.id_discr == dec_ci.id_discr);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_lai_and_lac()
{
	struct gsm0808_cell_id enc_ci = {
		.id_discr = CELL_IDENT_LAI_AND_LAC,
		.id.lai_and_lac = {
			.plmn = {
				.mcc = 123,
				.mnc = 456,
			},
			.lac = 0x2342,
		},
	};
	struct gsm0808_cell_id dec_ci;
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id(msg, &enc_ci);
	EXPECT_ENCODED("05 06 04 21 63 54 23 42");

	memset(&dec_ci, 0xa5, sizeof(dec_ci));
	rc_dec = gsm0808_dec_cell_id(&dec_ci, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == msg->len - 2);

	OSMO_ASSERT(enc_ci.id_discr == dec_ci.id_discr
		    && osmo_plmn_cmp(&enc_ci.id.lai_and_lac.plmn, &dec_ci.id.lai_and_lac.plmn) == 0
		    && enc_ci.id.lai_and_lac.lac == dec_ci.id.lai_and_lac.lac);
	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_ci()
{
	struct gsm0808_cell_id enc_ci = {
		.id_discr = CELL_IDENT_CI,
		.id.ci = 0x423,
	};
	struct gsm0808_cell_id dec_ci;
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id(msg, &enc_ci);
	EXPECT_ENCODED("05 03 02 04 23");

	rc_dec = gsm0808_dec_cell_id(&dec_ci, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == msg->len - 2);
	OSMO_ASSERT(enc_ci.id_discr == dec_ci.id_discr
		    && enc_ci.id.ci == dec_ci.id.ci);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_lac_and_ci()
{
	struct gsm0808_cell_id enc_ci = {
		.id_discr = CELL_IDENT_LAC_AND_CI,
		.id.lac_and_ci = {
			.lac = 0x423,
			.ci = 0x235,
		},
	};
	struct gsm0808_cell_id dec_ci;
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id(msg, &enc_ci);
	EXPECT_ENCODED("05 05 01 04 23 02 35");

	rc_dec = gsm0808_dec_cell_id(&dec_ci, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == msg->len - 2);
	OSMO_ASSERT(enc_ci.id_discr == dec_ci.id_discr
		    && enc_ci.id.lac_and_ci.lac == dec_ci.id.lac_and_ci.lac
		    && enc_ci.id.lac_and_ci.ci == dec_ci.id.lac_and_ci.ci);

	msgb_free(msg);
}

static void test_gsm0808_enc_dec_cell_id_global()
{
	struct gsm0808_cell_id enc_ci = {
		.id_discr = CELL_IDENT_WHOLE_GLOBAL,
		.id.global = {
			.lai = {
				.plmn = { .mcc = 123, .mnc = 456 },
				.lac = 0x2342
			},
			.cell_identity = 0x423,
		}
	};
	struct gsm0808_cell_id dec_ci;
	struct msgb *msg;
	uint8_t rc_enc;
	int rc_dec;

	msg = msgb_alloc(1024, "output buffer");
	rc_enc = gsm0808_enc_cell_id(msg, &enc_ci);
	EXPECT_ENCODED("05 08 00 21 63 54 23 42 04 23");

	rc_dec = gsm0808_dec_cell_id(&dec_ci, msg->data + 2, msg->len - 2);
	OSMO_ASSERT(rc_dec == msg->len - 2);

	OSMO_ASSERT(enc_ci.id_discr == dec_ci.id_discr
		    && osmo_plmn_cmp(&enc_ci.id.global.lai.plmn,
				     &dec_ci.id.global.lai.plmn) == 0
		    && enc_ci.id.global.lai.lac == dec_ci.id.global.lai.lac
		    && enc_ci.id.global.cell_identity == dec_ci.id.global.cell_identity);
	msgb_free(msg);
}

static void test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(struct gsm48_multi_rate_conf *cfg)
{
	uint16_t s15_s0;

	printf("Input:\n");
	printf(" m4_75= %u   smod=  %u\n", cfg->m4_75, cfg->smod);
	printf(" m5_15= %u   spare= %u\n", cfg->m5_15, cfg->spare);
	printf(" m5_90= %u   icmi=  %u\n", cfg->m5_90, cfg->icmi);
	printf(" m6_70= %u   nscb=  %u\n", cfg->m6_70, cfg->nscb);
	printf(" m7_40= %u   ver=   %u\n", cfg->m7_40, cfg->ver);
	printf(" m7_95= %u\n", cfg->m7_95);
	printf(" m10_2= %u\n", cfg->m10_2);
	printf(" m12_2= %u\n", cfg->m12_2);

	s15_s0 = gsm0808_sc_cfg_from_gsm48_mr_cfg(cfg, true);
	printf("Result (fr):\n");
	printf(" S15-S0 = %04x = 0b" OSMO_BIN_SPEC OSMO_BIN_SPEC "\n", s15_s0,
	       OSMO_BIN_PRINT(s15_s0 >> 8), OSMO_BIN_PRINT(s15_s0));

	s15_s0 = gsm0808_sc_cfg_from_gsm48_mr_cfg(cfg, false);
	printf("Result (hr):\n");
	printf(" S15-S0 = %04x = 0b" OSMO_BIN_SPEC OSMO_BIN_SPEC "\n", s15_s0,
	       OSMO_BIN_PRINT(s15_s0 >> 8), OSMO_BIN_PRINT(s15_s0));

	printf("\n");
}

static void test_gsm0808_sc_cfg_from_gsm48_mr_cfg(void)
{
	struct gsm48_multi_rate_conf cfg;

	printf("Testing gsm0808_sc_cfg_from_gsm48_mr_cfg():\n");

	memset(&cfg, 0, sizeof(cfg));

	cfg.m4_75 = 0;
	cfg.m5_15 = 0;
	cfg.m5_90 = 0;
	cfg.m6_70 = 0;
	cfg.m7_40 = 0;
	cfg.m7_95 = 0;
	cfg.m10_2 = 0;
	cfg.m12_2 = 0;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 1;
	cfg.m5_15 = 0;
	cfg.m5_90 = 0;
	cfg.m6_70 = 0;
	cfg.m7_40 = 0;
	cfg.m7_95 = 0;
	cfg.m10_2 = 0;
	cfg.m12_2 = 0;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 0;
	cfg.m5_15 = 1;
	cfg.m5_90 = 0;
	cfg.m6_70 = 0;
	cfg.m7_40 = 0;
	cfg.m7_95 = 0;
	cfg.m10_2 = 0;
	cfg.m12_2 = 0;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 0;
	cfg.m5_15 = 0;
	cfg.m5_90 = 1;
	cfg.m6_70 = 0;
	cfg.m7_40 = 0;
	cfg.m7_95 = 0;
	cfg.m10_2 = 0;
	cfg.m12_2 = 0;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 0;
	cfg.m5_15 = 0;
	cfg.m5_90 = 0;
	cfg.m6_70 = 1;
	cfg.m7_40 = 0;
	cfg.m7_95 = 0;
	cfg.m10_2 = 0;
	cfg.m12_2 = 0;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 0;
	cfg.m5_15 = 0;
	cfg.m5_90 = 0;
	cfg.m6_70 = 0;
	cfg.m7_40 = 1;
	cfg.m7_95 = 0;
	cfg.m10_2 = 0;
	cfg.m12_2 = 0;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 0;
	cfg.m5_15 = 0;
	cfg.m5_90 = 0;
	cfg.m6_70 = 0;
	cfg.m7_40 = 0;
	cfg.m7_95 = 1;
	cfg.m10_2 = 0;
	cfg.m12_2 = 0;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 0;
	cfg.m5_15 = 0;
	cfg.m5_90 = 0;
	cfg.m6_70 = 0;
	cfg.m7_40 = 0;
	cfg.m7_95 = 0;
	cfg.m10_2 = 1;
	cfg.m12_2 = 0;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 0;
	cfg.m5_15 = 0;
	cfg.m5_90 = 0;
	cfg.m6_70 = 0;
	cfg.m7_40 = 0;
	cfg.m7_95 = 0;
	cfg.m10_2 = 0;
	cfg.m12_2 = 1;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 1;
	cfg.m5_15 = 1;
	cfg.m5_90 = 1;
	cfg.m6_70 = 1;
	cfg.m7_40 = 0;
	cfg.m7_95 = 0;
	cfg.m10_2 = 0;
	cfg.m12_2 = 0;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 0;
	cfg.m5_15 = 0;
	cfg.m5_90 = 0;
	cfg.m6_70 = 0;
	cfg.m7_40 = 1;
	cfg.m7_95 = 1;
	cfg.m10_2 = 1;
	cfg.m12_2 = 1;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 0;
	cfg.m5_15 = 0;
	cfg.m5_90 = 1;
	cfg.m6_70 = 1;
	cfg.m7_40 = 0;
	cfg.m7_95 = 0;
	cfg.m10_2 = 1;
	cfg.m12_2 = 1;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 1;
	cfg.m5_15 = 1;
	cfg.m5_90 = 0;
	cfg.m6_70 = 0;
	cfg.m7_40 = 1;
	cfg.m7_95 = 1;
	cfg.m10_2 = 0;
	cfg.m12_2 = 0;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 0;
	cfg.m5_15 = 1;
	cfg.m5_90 = 0;
	cfg.m6_70 = 1;
	cfg.m7_40 = 0;
	cfg.m7_95 = 1;
	cfg.m10_2 = 0;
	cfg.m12_2 = 1;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 1;
	cfg.m5_15 = 0;
	cfg.m5_90 = 1;
	cfg.m6_70 = 0;
	cfg.m7_40 = 1;
	cfg.m7_95 = 0;
	cfg.m10_2 = 1;
	cfg.m12_2 = 0;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);

	cfg.m4_75 = 1;
	cfg.m5_15 = 1;
	cfg.m5_90 = 1;
	cfg.m6_70 = 1;
	cfg.m7_40 = 1;
	cfg.m7_95 = 1;
	cfg.m10_2 = 1;
	cfg.m12_2 = 1;
	test_gsm0808_sc_cfg_from_gsm48_mr_cfg_single(&cfg);
}

static void test_gsm48_mr_cfg_from_gsm0808_sc_cfg_single(uint16_t s15_s0)
{
	struct gsm48_multi_rate_conf cfg;

	printf("Input:\n");
	printf(" S15-S0 = %04x = 0b" OSMO_BIN_SPEC OSMO_BIN_SPEC "\n", s15_s0,
	       OSMO_BIN_PRINT(s15_s0 >> 8), OSMO_BIN_PRINT(s15_s0));

	gsm48_mr_cfg_from_gsm0808_sc_cfg(&cfg, s15_s0);

	printf("Output:\n");
	printf(" m4_75= %u   smod=  %u\n", cfg.m4_75, cfg.smod);
	printf(" m5_15= %u   spare= %u\n", cfg.m5_15, cfg.spare);
	printf(" m5_90= %u   icmi=  %u\n", cfg.m5_90, cfg.icmi);
	printf(" m6_70= %u   nscb=  %u\n", cfg.m6_70, cfg.nscb);
	printf(" m7_40= %u   ver=   %u\n", cfg.m7_40, cfg.ver);
	printf(" m7_95= %u\n", cfg.m7_95);
	printf(" m10_2= %u\n", cfg.m10_2);
	printf(" m12_2= %u\n", cfg.m12_2);

	printf("\n");
}

void test_gsm48_mr_cfg_from_gsm0808_sc_cfg()
{
	printf("Testing gsm48_mr_cfg_from_gsm0808_sc_cfg():\n");

	/* Only one codec per setting */
	test_gsm48_mr_cfg_from_gsm0808_sc_cfg_single
	    (GSM0808_SC_CFG_DEFAULT_AMR_4_75);
	test_gsm48_mr_cfg_from_gsm0808_sc_cfg_single
	    (GSM0808_SC_CFG_DEFAULT_AMR_5_15);
	test_gsm48_mr_cfg_from_gsm0808_sc_cfg_single
	    (GSM0808_SC_CFG_DEFAULT_AMR_5_90);
	test_gsm48_mr_cfg_from_gsm0808_sc_cfg_single
	    (GSM0808_SC_CFG_DEFAULT_AMR_6_70);
	test_gsm48_mr_cfg_from_gsm0808_sc_cfg_single
	    (GSM0808_SC_CFG_DEFAULT_AMR_7_40);
	test_gsm48_mr_cfg_from_gsm0808_sc_cfg_single
	    (GSM0808_SC_CFG_DEFAULT_AMR_7_95);
	test_gsm48_mr_cfg_from_gsm0808_sc_cfg_single
	    (GSM0808_SC_CFG_DEFAULT_AMR_10_2);
	test_gsm48_mr_cfg_from_gsm0808_sc_cfg_single
	    (GSM0808_SC_CFG_DEFAULT_AMR_12_2);

	/* Combinations */
	test_gsm48_mr_cfg_from_gsm0808_sc_cfg_single
	    (GSM0808_SC_CFG_DEFAULT_AMR_4_75 | GSM0808_SC_CFG_DEFAULT_AMR_6_70 |
	     GSM0808_SC_CFG_DEFAULT_AMR_10_2);
	test_gsm48_mr_cfg_from_gsm0808_sc_cfg_single
	    (GSM0808_SC_CFG_DEFAULT_AMR_10_2 | GSM0808_SC_CFG_DEFAULT_AMR_12_2 |
	     GSM0808_SC_CFG_DEFAULT_AMR_7_40);
	test_gsm48_mr_cfg_from_gsm0808_sc_cfg_single
	    (GSM0808_SC_CFG_DEFAULT_AMR_7_95 | GSM0808_SC_CFG_DEFAULT_AMR_12_2);
}

int main(int argc, char **argv)
{
	void *ctx = talloc_named_const(NULL, 0, "gsm0808 test");
	msgb_talloc_ctx_init(ctx, 0);
	osmo_init_logging2(ctx, NULL);

	printf("Testing generation of GSM0808 messages\n");
	test_gsm0808_enc_cause();
	test_create_layer3();
	test_create_layer3_aoip();
	test_create_reset();
	test_create_reset_ack();
	test_create_clear_command();
	test_create_clear_complete();
	test_create_cipher();
	test_create_cipher_complete();
	test_create_cipher_reject();
	test_create_cipher_reject_ext();
	test_create_cm_u();
	test_create_sapi_reject();
	test_create_ass();
	test_create_ass2();
	test_create_ass_compl();
	test_create_ass_compl_aoip();
	test_create_ass_fail();
	test_create_ass_fail_aoip();
	test_create_clear_rqst();
	test_create_paging();
	test_create_dtap();
	test_prepend_dtap();

	test_enc_dec_lcls();

	test_enc_dec_aoip_trasp_addr_v4();
	test_enc_dec_aoip_trasp_addr_v6();
	test_gsm0808_enc_dec_speech_codec();
	test_gsm0808_enc_dec_speech_codec_ext_with_cfg();
	test_gsm0808_enc_dec_speech_codec_with_cfg();
	test_gsm0808_enc_dec_speech_codec_list();
	test_gsm0808_enc_dec_empty_speech_codec_list();
	test_gsm0808_enc_dec_channel_type();
	test_gsm0808_enc_dec_encrypt_info();

	test_gsm0808_enc_dec_cell_id_list_lac();
	test_gsm0808_enc_dec_cell_id_list_single_lac();
	test_gsm0808_enc_dec_cell_id_list_multi_lac();
	test_gsm0808_enc_dec_cell_id_list_bss();
	test_gsm0808_enc_dec_cell_id_list_multi_lai_and_lac();
	test_gsm0808_enc_dec_cell_id_list_multi_ci();
	test_gsm0808_enc_dec_cell_id_list_multi_lac_and_ci();
	test_gsm0808_enc_dec_cell_id_list_multi_global();

	test_cell_id_list_add();

	test_gsm0808_enc_dec_cell_id_lac();
	test_gsm0808_enc_dec_cell_id_bss();
	test_gsm0808_enc_dec_cell_id_no_cell();
	test_gsm0808_enc_dec_cell_id_lai_and_lac();
	test_gsm0808_enc_dec_cell_id_ci();
	test_gsm0808_enc_dec_cell_id_lac_and_ci();
	test_gsm0808_enc_dec_cell_id_global();

	test_gsm0808_sc_cfg_from_gsm48_mr_cfg();
	test_gsm48_mr_cfg_from_gsm0808_sc_cfg();

	printf("Done\n");
	return EXIT_SUCCESS;
}
