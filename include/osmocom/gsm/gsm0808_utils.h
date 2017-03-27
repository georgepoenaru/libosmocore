/* (C) 2016 by Sysmocom s.f.m.c. GmbH
 * All Rights Reserved
 *
 * Author: Philipp Maier
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#pragma once

#include <sys/socket.h>

#include <osmocom/gsm/protocol/gsm_08_08.h>

/* Encode AoIP transport address element */
uint8_t gsm0808_enc_aoip_trasp_addr(struct msgb *msg,
				    const struct sockaddr_storage *ss);

/* Decode AoIP transport address element */
int gsm0808_dec_aoip_trasp_addr(struct sockaddr_storage *ss,
				const uint8_t *elem, uint8_t len);

/* Encode Speech Codec element */
uint8_t gsm0808_enc_speech_codec(struct msgb *msg,
				 const struct gsm0808_speech_codec *sc);

/* Decode Speech Codec element */
int gsm0808_dec_speech_codec(struct gsm0808_speech_codec *sc,
			     const uint8_t *elem, uint8_t len);

/* Encode Speech Codec list */
uint8_t gsm0808_enc_speech_codec_list(struct msgb *msg,
				      const struct gsm0808_speech_codec_list
				      *scl);

/* Decode Speech Codec list */
int gsm0808_dec_speech_codec_list(struct gsm0808_speech_codec_list *scl,
				  const uint8_t *elem, uint8_t len);