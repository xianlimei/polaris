/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)rsa.c	1.6	05/10/07 SMI"

/*
 * RSA provider for the Kernel Cryptographic Framework (KCF)
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/modctl.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/crypto/spi.h>
#include <sys/sysmacros.h>
#include <sys/strsun.h>
#include <sys/md5.h>
#include <sys/sha1.h>
#include <sys/sha2.h>
#include <sys/random.h>
#include "rsa_impl.h"

extern struct mod_ops mod_cryptoops;

/*
 * Module linkage information for the kernel.
 */
static struct modlcrypto modlcrypto = {
	&mod_cryptoops,
	"RSA Kernel SW Provider 1.6"
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modlcrypto,
	NULL
};

/*
 * CSPI information (entry points, provider info, etc.)
 */
typedef enum rsa_mech_type {
	RSA_PKCS_MECH_INFO_TYPE,	/* SUN_CKM_RSA_PKCS */
	RSA_X_509_MECH_INFO_TYPE,	/* SUN_CKM_RSA_X_509 */
	MD5_RSA_PKCS_MECH_INFO_TYPE,	/* SUN_MD5_RSA_PKCS */
	SHA1_RSA_PKCS_MECH_INFO_TYPE,	/* SUN_SHA1_RSA_PKCS */
	SHA256_RSA_PKCS_MECH_INFO_TYPE,	/* SUN_SHA256_RSA_PKCS */
	SHA384_RSA_PKCS_MECH_INFO_TYPE,	/* SUN_SHA384_RSA_PKCS */
	SHA512_RSA_PKCS_MECH_INFO_TYPE	/* SUN_SHA512_RSA_PKCS */
} rsa_mech_type_t;

/*
 * Context for RSA_PKCS and RSA_X_509 mechanisms.
 */
typedef struct rsa_ctx {
	rsa_mech_type_t	mech_type;
	crypto_key_t *key;
	size_t keychunk_size;
} rsa_ctx_t;

/*
 * Context for MD5_RSA_PKCS and SHA*_RSA_PKCS mechanisms.
 */
typedef struct digest_rsa_ctx {
	rsa_mech_type_t	mech_type;
	crypto_key_t *key;
	size_t keychunk_size;
	union {
		MD5_CTX md5ctx;
		SHA1_CTX sha1ctx;
		SHA2_CTX sha2ctx;
	} dctx_u;
} digest_rsa_ctx_t;

#define	md5_ctx		dctx_u.md5ctx
#define	sha1_ctx	dctx_u.sha1ctx
#define	sha2_ctx	dctx_u.sha2ctx

/*
 * Mechanism info structure passed to KCF during registration.
 */
static crypto_mech_info_t rsa_mech_info_tab[] = {
	/* RSA_PKCS */
	{SUN_CKM_RSA_PKCS, RSA_PKCS_MECH_INFO_TYPE,
	    CRYPTO_FG_ENCRYPT | CRYPTO_FG_ENCRYPT_ATOMIC |
	    CRYPTO_FG_DECRYPT | CRYPTO_FG_DECRYPT_ATOMIC |
	    CRYPTO_FG_SIGN | CRYPTO_FG_SIGN_ATOMIC |
	    CRYPTO_FG_VERIFY | CRYPTO_FG_VERIFY_ATOMIC |
	    CRYPTO_FG_SIGN_RECOVER | CRYPTO_FG_SIGN_RECOVER_ATOMIC |
	    CRYPTO_FG_VERIFY_RECOVER | CRYPTO_FG_VERIFY_RECOVER_ATOMIC,
	    RSA_MIN_KEY_LEN, RSA_MAX_KEY_LEN, CRYPTO_KEYSIZE_UNIT_IN_BITS},

	/* RSA_X_509 */
	{SUN_CKM_RSA_X_509, RSA_X_509_MECH_INFO_TYPE,
	    CRYPTO_FG_ENCRYPT | CRYPTO_FG_ENCRYPT_ATOMIC |
	    CRYPTO_FG_DECRYPT | CRYPTO_FG_DECRYPT_ATOMIC |
	    CRYPTO_FG_SIGN | CRYPTO_FG_SIGN_ATOMIC |
	    CRYPTO_FG_VERIFY | CRYPTO_FG_VERIFY_ATOMIC |
	    CRYPTO_FG_SIGN_RECOVER | CRYPTO_FG_SIGN_RECOVER_ATOMIC |
	    CRYPTO_FG_VERIFY_RECOVER | CRYPTO_FG_VERIFY_RECOVER_ATOMIC,
	    RSA_MIN_KEY_LEN, RSA_MAX_KEY_LEN, CRYPTO_KEYSIZE_UNIT_IN_BITS},

	/* MD5_RSA_PKCS */
	{SUN_CKM_MD5_RSA_PKCS, MD5_RSA_PKCS_MECH_INFO_TYPE,
	    CRYPTO_FG_SIGN | CRYPTO_FG_SIGN_ATOMIC |
	    CRYPTO_FG_VERIFY | CRYPTO_FG_VERIFY_ATOMIC,
	    RSA_MIN_KEY_LEN, RSA_MAX_KEY_LEN, CRYPTO_KEYSIZE_UNIT_IN_BITS},

	/* SHA1_RSA_PKCS */
	{SUN_CKM_SHA1_RSA_PKCS, SHA1_RSA_PKCS_MECH_INFO_TYPE,
	    CRYPTO_FG_SIGN | CRYPTO_FG_SIGN_ATOMIC |
	    CRYPTO_FG_VERIFY | CRYPTO_FG_VERIFY_ATOMIC,
	    RSA_MIN_KEY_LEN, RSA_MAX_KEY_LEN, CRYPTO_KEYSIZE_UNIT_IN_BITS},

	/* SHA256_RSA_PKCS */
	{SUN_CKM_SHA256_RSA_PKCS, SHA256_RSA_PKCS_MECH_INFO_TYPE,
	    CRYPTO_FG_SIGN | CRYPTO_FG_SIGN_ATOMIC |
	    CRYPTO_FG_VERIFY | CRYPTO_FG_VERIFY_ATOMIC,
	    RSA_MIN_KEY_LEN, RSA_MAX_KEY_LEN, CRYPTO_KEYSIZE_UNIT_IN_BITS},

	/* SHA384_RSA_PKCS */
	{SUN_CKM_SHA384_RSA_PKCS, SHA384_RSA_PKCS_MECH_INFO_TYPE,
	    CRYPTO_FG_SIGN | CRYPTO_FG_SIGN_ATOMIC |
	    CRYPTO_FG_VERIFY | CRYPTO_FG_VERIFY_ATOMIC,
	    RSA_MIN_KEY_LEN, RSA_MAX_KEY_LEN, CRYPTO_KEYSIZE_UNIT_IN_BITS},

	/* SHA512_RSA_PKCS */
	{SUN_CKM_SHA512_RSA_PKCS, SHA512_RSA_PKCS_MECH_INFO_TYPE,
	    CRYPTO_FG_SIGN | CRYPTO_FG_SIGN_ATOMIC |
	    CRYPTO_FG_VERIFY | CRYPTO_FG_VERIFY_ATOMIC,
	    RSA_MIN_KEY_LEN, RSA_MAX_KEY_LEN, CRYPTO_KEYSIZE_UNIT_IN_BITS}

};

#define	RSA_VALID_MECH(mech)					\
	(((mech)->cm_type == RSA_PKCS_MECH_INFO_TYPE ||		\
	(mech)->cm_type == RSA_X_509_MECH_INFO_TYPE ||		\
	(mech)->cm_type == MD5_RSA_PKCS_MECH_INFO_TYPE ||	\
	(mech)->cm_type == SHA1_RSA_PKCS_MECH_INFO_TYPE ||	\
	(mech)->cm_type == SHA256_RSA_PKCS_MECH_INFO_TYPE ||	\
	(mech)->cm_type == SHA384_RSA_PKCS_MECH_INFO_TYPE ||	\
	(mech)->cm_type == SHA512_RSA_PKCS_MECH_INFO_TYPE) ? 1 : 0)

/* operations are in-place if the output buffer is NULL */
#define	RSA_ARG_INPLACE(input, output)				\
	if ((output) == NULL)					\
		(output) = (input);

static void rsa_provider_status(crypto_provider_handle_t, uint_t *);

static crypto_control_ops_t rsa_control_ops = {
	rsa_provider_status
};

static int rsa_common_init(crypto_ctx_t *, crypto_mechanism_t *,
    crypto_key_t *, crypto_spi_ctx_template_t, crypto_req_handle_t);
static int rsa_encrypt(crypto_ctx_t *, crypto_data_t *, crypto_data_t *,
    crypto_req_handle_t);
static int rsa_encrypt_atomic(crypto_provider_handle_t, crypto_session_id_t,
    crypto_mechanism_t *, crypto_key_t *, crypto_data_t *,
    crypto_data_t *, crypto_spi_ctx_template_t, crypto_req_handle_t);
static int rsa_decrypt(crypto_ctx_t *, crypto_data_t *, crypto_data_t *,
    crypto_req_handle_t);
static int rsa_decrypt_atomic(crypto_provider_handle_t, crypto_session_id_t,
    crypto_mechanism_t *, crypto_key_t *, crypto_data_t *,
    crypto_data_t *, crypto_spi_ctx_template_t, crypto_req_handle_t);

/*
 * The RSA mechanisms do not have multiple-part cipher operations.
 * So, the update and final routines are set to NULL.
 */
static crypto_cipher_ops_t rsa_cipher_ops = {
	rsa_common_init,
	rsa_encrypt,
	NULL,
	NULL,
	rsa_encrypt_atomic,
	rsa_common_init,
	rsa_decrypt,
	NULL,
	NULL,
	rsa_decrypt_atomic
};

static int rsa_sign_verify_common_init(crypto_ctx_t *, crypto_mechanism_t *,
    crypto_key_t *, crypto_spi_ctx_template_t, crypto_req_handle_t);
static int rsa_sign(crypto_ctx_t *, crypto_data_t *, crypto_data_t *,
    crypto_req_handle_t);
static int rsa_sign_update(crypto_ctx_t *, crypto_data_t *,
    crypto_req_handle_t);
static int rsa_sign_final(crypto_ctx_t *, crypto_data_t *,
    crypto_req_handle_t);
static int rsa_sign_atomic(crypto_provider_handle_t, crypto_session_id_t,
    crypto_mechanism_t *, crypto_key_t *, crypto_data_t *, crypto_data_t *,
    crypto_spi_ctx_template_t, crypto_req_handle_t);

/*
 * We use the same routine for sign_init and sign_recover_init fields
 * as they do the same thing. Same holds for sign and sign_recover fields,
 * and sign_atomic and sign_recover_atomic fields.
 */
static crypto_sign_ops_t rsa_sign_ops = {
	rsa_sign_verify_common_init,
	rsa_sign,
	rsa_sign_update,
	rsa_sign_final,
	rsa_sign_atomic,
	rsa_sign_verify_common_init,
	rsa_sign,
	rsa_sign_atomic
};

static int rsa_verify(crypto_ctx_t *, crypto_data_t *, crypto_data_t *,
    crypto_req_handle_t);
static int rsa_verify_update(crypto_ctx_t *, crypto_data_t *,
    crypto_req_handle_t);
static int rsa_verify_final(crypto_ctx_t *, crypto_data_t *,
    crypto_req_handle_t);
static int rsa_verify_atomic(crypto_provider_handle_t, crypto_session_id_t,
    crypto_mechanism_t *, crypto_key_t *, crypto_data_t *,
    crypto_data_t *, crypto_spi_ctx_template_t, crypto_req_handle_t);
static int rsa_verify_recover(crypto_ctx_t *, crypto_data_t *,
    crypto_data_t *, crypto_req_handle_t);
static int rsa_verify_recover_atomic(crypto_provider_handle_t,
    crypto_session_id_t, crypto_mechanism_t *, crypto_key_t *,
    crypto_data_t *, crypto_data_t *, crypto_spi_ctx_template_t,
    crypto_req_handle_t);

/*
 * We use the same routine (rsa_sign_verify_common_init) for verify_init
 * and verify_recover_init fields as they do the same thing.
 */
static crypto_verify_ops_t rsa_verify_ops = {
	rsa_sign_verify_common_init,
	rsa_verify,
	rsa_verify_update,
	rsa_verify_final,
	rsa_verify_atomic,
	rsa_sign_verify_common_init,
	rsa_verify_recover,
	rsa_verify_recover_atomic
};

static int rsa_free_context(crypto_ctx_t *);

static crypto_ctx_ops_t rsa_ctx_ops = {
	NULL,
	rsa_free_context
};

static crypto_ops_t rsa_crypto_ops = {
	&rsa_control_ops,
	NULL,
	&rsa_cipher_ops,
	NULL,
	&rsa_sign_ops,
	&rsa_verify_ops,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&rsa_ctx_ops
};

static crypto_provider_info_t rsa_prov_info = {
	CRYPTO_SPI_VERSION_1,
	"RSA Software Provider",
	CRYPTO_SW_PROVIDER,
	{&modlinkage},
	NULL,
	&rsa_crypto_ops,
	sizeof (rsa_mech_info_tab)/sizeof (crypto_mech_info_t),
	rsa_mech_info_tab
};

static int rsa_encrypt_common(rsa_mech_type_t, crypto_key_t *,
    crypto_data_t *, crypto_data_t *, int);
static int rsa_decrypt_common(rsa_mech_type_t, crypto_key_t *,
    crypto_data_t *, crypto_data_t *, int);
static int rsa_sign_common(rsa_mech_type_t, crypto_key_t *,
    crypto_data_t *, crypto_data_t *, int);
static int rsa_verify_common(rsa_mech_type_t, crypto_key_t *,
    crypto_data_t *, crypto_data_t *, int);
static int compare_data(crypto_data_t *, uchar_t *);

/* EXPORT DELETE START */

static int core_rsa_encrypt(crypto_key_t *, uchar_t *,
    int, uchar_t *, int, int);
static int core_rsa_decrypt(crypto_key_t *, uchar_t *, int,
    uchar_t *, int);

/* EXPORT DELETE END */

static crypto_kcf_provider_handle_t rsa_prov_handle = NULL;

int
_init(void)
{
	int ret;

	/*
	 * Register with KCF. If the registration fails, return error.
	 */
	if ((ret = crypto_register_provider(&rsa_prov_info,
	    &rsa_prov_handle)) != CRYPTO_SUCCESS) {
		cmn_err(CE_WARN, "rsa _init: crypto_register_provider()"
		    "failed (0x%x)", ret);
		return (EACCES);
	}

	if ((ret = mod_install(&modlinkage)) != 0) {
		int rv;

		ASSERT(rsa_prov_handle != NULL);
		/* We should not return if the unregister returns busy. */
		while ((rv = crypto_unregister_provider(rsa_prov_handle))
		    == CRYPTO_BUSY) {
			cmn_err(CE_WARN, "rsa _init: "
			    "crypto_unregister_provider() "
			    "failed (0x%x). Retrying.", rv);
			/* wait 10 seconds and try again. */
			delay(10 * drv_usectohz(1000000));
		}
	}

	return (ret);
}

int
_fini(void)
{
	int ret;

	/*
	 * Unregister from KCF if previous registration succeeded.
	 */
	if (rsa_prov_handle != NULL) {
		if ((ret = crypto_unregister_provider(rsa_prov_handle)) !=
		    CRYPTO_SUCCESS) {
			cmn_err(CE_WARN, "rsa _fini: "
			    "crypto_unregister_provider() "
			    "failed (0x%x)", ret);
			return (EBUSY);
		}
		rsa_prov_handle = NULL;
	}

	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/* ARGSUSED */
static void
rsa_provider_status(crypto_provider_handle_t provider, uint_t *status)
{
	*status = CRYPTO_PROVIDER_READY;
}

/*
 * Utility routine to look up a attribute of type, 'type',
 * in the key.
 */
static int
get_key_attr(crypto_key_t *key, crypto_attr_type_t type,
    uchar_t **value, ssize_t *value_len)
{
	int i;

	ASSERT(key->ck_format == CRYPTO_KEY_ATTR_LIST);
	for (i = 0; i < key->ck_count; i++) {
		if (key->ck_attrs[i].oa_type == type) {
			*value = (uchar_t *)key->ck_attrs[i].oa_value;
			*value_len = key->ck_attrs[i].oa_value_len;
			return (CRYPTO_SUCCESS);
		}
	}

	return (CRYPTO_FAILED);
}

static int
check_mech_and_key(crypto_mechanism_t *mechanism, crypto_key_t *key)
{
	int rv = CRYPTO_FAILED;

/* EXPORT DELETE START */

	uchar_t *modulus;
	ssize_t modulus_len; /* In bytes */

	if (!RSA_VALID_MECH(mechanism))
		return (CRYPTO_MECHANISM_INVALID);

	/*
	 * We only support RSA keys that are passed as a list of
	 * object attributes.
	 */
	if (key->ck_format != CRYPTO_KEY_ATTR_LIST) {
		return (CRYPTO_KEY_TYPE_INCONSISTENT);
	}

	if ((rv = get_key_attr(key, SUN_CKA_MODULUS, &modulus,
	    &modulus_len)) != CRYPTO_SUCCESS) {
		return (rv);
	}
	if (modulus_len < MIN_RSA_KEYLENGTH_IN_BYTES ||
	    modulus_len > MAX_RSA_KEYLENGTH_IN_BYTES)
		return (CRYPTO_KEY_SIZE_RANGE);

/* EXPORT DELETE END */

	return (rv);
}

void
kmemset(uint8_t *buf, char pattern, size_t len)
{
	int i = 0;

	while (i < len)
		buf[i++] = pattern;
}

/*
 * This function guarantees to return non-zero random numbers.
 * This is needed as the /dev/urandom kernel interface,
 * random_get_pseudo_bytes(), may return zeros.
 */
int
knzero_random_generator(uint8_t *ran_out, size_t ran_len)
{
	int rv;
	size_t ebc = 0; /* count of extra bytes in extrarand */
	size_t i = 0;
	uint8_t extrarand[32];
	size_t extrarand_len;

	if ((rv = random_get_pseudo_bytes(ran_out, ran_len)) != 0)
		return (rv);

	/*
	 * Walk through the returned random numbers pointed by ran_out,
	 * and look for any random number which is zero.
	 * If we find zero, call random_get_pseudo_bytes() to generate
	 * another 32 random numbers pool. Replace any zeros in ran_out[]
	 * from the random number in pool.
	 */
	while (i < ran_len) {
		if (ran_out[i] != 0) {
			i++;
			continue;
		}

		/*
		 * Note that it is 'while' so we are guaranteed a
		 * non-zero value on exit.
		 */
		if (ebc == 0) {
			/* refresh extrarand */
			extrarand_len = sizeof (extrarand);
			if ((rv = random_get_pseudo_bytes(extrarand,
			    extrarand_len)) != 0) {
				return (rv);
			}

			ebc = extrarand_len;
		}
		/* Replace zero with byte from extrarand. */
		-- ebc;

		/*
		 * The new random byte zero/non-zero will be checked in
		 * the next pass through the loop.
		 */
		ran_out[i] = extrarand[ebc];
	}

	return (CRYPTO_SUCCESS);
}

typedef enum cmd_type {
	COPY_FROM_DATA,
	COPY_TO_DATA,
	COMPARE_TO_DATA,
	MD5_DIGEST_DATA,
	SHA1_DIGEST_DATA,
	SHA2_DIGEST_DATA
} cmd_type_t;

/*
 * Utility routine to apply the command, 'cmd', to the
 * data in the uio structure.
 */
static int
process_uio_data(crypto_data_t *data, uchar_t *buf, int len,
    cmd_type_t cmd, void *digest_ctx)
{
	uio_t *uiop = data->cd_uio;
	off_t offset = data->cd_offset;
	size_t length = len;
	uint_t vec_idx;
	size_t cur_len;
	uchar_t *datap;

	ASSERT(data->cd_format == CRYPTO_DATA_UIO);
	if (uiop->uio_segflg != UIO_SYSSPACE) {
		return (CRYPTO_ARGUMENTS_BAD);
	}

	/*
	 * Jump to the first iovec containing data to be
	 * processed.
	 */
	for (vec_idx = 0; vec_idx < uiop->uio_iovcnt &&
	    offset >= uiop->uio_iov[vec_idx].iov_len;
	    offset -= uiop->uio_iov[vec_idx++].iov_len);

	if (vec_idx == uiop->uio_iovcnt) {
		/*
		 * The caller specified an offset that is larger than
		 * the total size of the buffers it provided.
		 */
		return (CRYPTO_DATA_LEN_RANGE);
	}

	while (vec_idx < uiop->uio_iovcnt && length > 0) {
		cur_len = MIN(uiop->uio_iov[vec_idx].iov_len -
		    offset, length);

		datap = (uchar_t *)(uiop->uio_iov[vec_idx].iov_base +
		    offset);
		switch (cmd) {
		case COPY_FROM_DATA:
			bcopy(datap, buf, cur_len);
			buf += cur_len;
			break;
		case COPY_TO_DATA:
			bcopy(buf, datap, cur_len);
			buf += cur_len;
			break;
		case COMPARE_TO_DATA:
			if (bcmp(datap, buf, cur_len))
				return (CRYPTO_SIGNATURE_INVALID);
			buf += cur_len;
			break;
		case MD5_DIGEST_DATA:
			MD5Update(digest_ctx, datap, cur_len);
			break;
		case SHA1_DIGEST_DATA:
			SHA1Update(digest_ctx, datap, cur_len);
			break;
		case SHA2_DIGEST_DATA:
			SHA2Update(digest_ctx, datap, cur_len);
			break;
		}

		length -= cur_len;
		vec_idx++;
		offset = 0;
	}

	if (vec_idx == uiop->uio_iovcnt && length > 0) {
		/*
		 * The end of the specified iovec's was reached but
		 * the length requested could not be processed.
		 */
		switch (cmd) {
		case COPY_TO_DATA:
			data->cd_length = len;
			return (CRYPTO_BUFFER_TOO_SMALL);
		default:
			return (CRYPTO_DATA_LEN_RANGE);
		}
	}

	return (CRYPTO_SUCCESS);
}

/*
 * Utility routine to apply the command, 'cmd', to the
 * data in the mblk structure.
 */
static int
process_mblk_data(crypto_data_t *data, uchar_t *buf, int len,
    cmd_type_t cmd, void *digest_ctx)
{
	off_t offset = data->cd_offset;
	size_t length = len;
	mblk_t *mp;
	size_t cur_len;
	uchar_t *datap;

	ASSERT(data->cd_format == CRYPTO_DATA_MBLK);
	/*
	 * Jump to the first mblk_t containing data to be processed.
	 */
	for (mp = data->cd_mp; mp != NULL && offset >= MBLKL(mp);
	    offset -= MBLKL(mp), mp = mp->b_cont);
	if (mp == NULL) {
		/*
		 * The caller specified an offset that is larger
		 * than the total size of the buffers it provided.
		 */
		return (CRYPTO_DATA_LEN_RANGE);
	}

	/*
	 * Now do the processing on the mblk chain.
	 */
	while (mp != NULL && length > 0) {
		cur_len = MIN(MBLKL(mp) - offset, length);

		datap = (uchar_t *)(mp->b_rptr + offset);
		switch (cmd) {
		case COPY_FROM_DATA:
			bcopy(datap, buf, cur_len);
			buf += cur_len;
			break;
		case COPY_TO_DATA:
			bcopy(buf, datap, cur_len);
			buf += cur_len;
			break;
		case COMPARE_TO_DATA:
			if (bcmp(datap, buf, cur_len))
				return (CRYPTO_SIGNATURE_INVALID);
			buf += cur_len;
			break;
		case MD5_DIGEST_DATA:
			MD5Update(digest_ctx, datap, cur_len);
			break;
		case SHA1_DIGEST_DATA:
			SHA1Update(digest_ctx, datap, cur_len);
			break;
		case SHA2_DIGEST_DATA:
			SHA2Update(digest_ctx, datap, cur_len);
			break;
		}

		length -= cur_len;
		offset = 0;
		mp = mp->b_cont;
	}

	if (mp == NULL && length > 0) {
		/*
		 * The end of the mblk was reached but the length
		 * requested could not be processed.
		 */
		switch (cmd) {
		case COPY_TO_DATA:
			data->cd_length = len;
			return (CRYPTO_BUFFER_TOO_SMALL);
		default:
			return (CRYPTO_DATA_LEN_RANGE);
		}
	}

	return (CRYPTO_SUCCESS);
}

static int
compare_data(crypto_data_t *data, uchar_t *buf)
{
	int len;
	uchar_t *dptr;

	len = data->cd_length;
	switch (data->cd_format) {
	case CRYPTO_DATA_RAW:
		dptr = (uchar_t *)(data->cd_raw.iov_base +
		    data->cd_offset);

		return (bcmp(dptr, buf, len));

	case CRYPTO_DATA_UIO:
		return (process_uio_data(data, buf, len,
		    COMPARE_TO_DATA, NULL));

	case CRYPTO_DATA_MBLK:
		return (process_mblk_data(data, buf, len,
		    COMPARE_TO_DATA, NULL));
	}

	return (CRYPTO_FAILED);
}

/*
 * Utility routine to copy a buffer to a crypto_data structure.
 */
static int
put_output_data(uchar_t *buf, crypto_data_t *output, int len)
{
	switch (output->cd_format) {
	case CRYPTO_DATA_RAW:
		if (output->cd_raw.iov_len < len) {
			output->cd_length = len;
			return (CRYPTO_BUFFER_TOO_SMALL);
		}
		bcopy(buf, (uchar_t *)(output->cd_raw.iov_base +
		    output->cd_offset), len);
		break;

	case CRYPTO_DATA_UIO:
		return (process_uio_data(output, buf, len, COPY_TO_DATA, NULL));

	case CRYPTO_DATA_MBLK:
		return (process_mblk_data(output, buf, len,
		    COPY_TO_DATA, NULL));

	default:
		return (CRYPTO_ARGUMENTS_BAD);
	}

	return (CRYPTO_SUCCESS);
}

/*
 * Utility routine to get data from a crypto_data structure.
 *
 * '*dptr' contains a pointer to a buffer on return. 'buf'
 * is allocated by the caller and is ignored for CRYPTO_DATA_RAW case.
 */
static int
get_input_data(crypto_data_t *input, uchar_t **dptr, uchar_t *buf)
{
	int rv;

	switch (input->cd_format) {
	case CRYPTO_DATA_RAW:
		if (input->cd_raw.iov_len < input->cd_length)
			return (CRYPTO_ARGUMENTS_BAD);
		*dptr = (uchar_t *)(input->cd_raw.iov_base +
		    input->cd_offset);
		break;

	case CRYPTO_DATA_UIO:
		if ((rv = process_uio_data(input, buf, input->cd_length,
		    COPY_FROM_DATA, NULL)) != CRYPTO_SUCCESS)
			return (rv);
		*dptr = buf;
		break;

	case CRYPTO_DATA_MBLK:
		if ((rv = process_mblk_data(input, buf, input->cd_length,
		    COPY_FROM_DATA, NULL)) != CRYPTO_SUCCESS)
			return (rv);
		*dptr = buf;
		break;

	default:
		return (CRYPTO_ARGUMENTS_BAD);
	}

	return (CRYPTO_SUCCESS);
}

static int
copy_key_to_ctx(crypto_key_t *in_key, rsa_ctx_t *ctx, int kmflag)
{
	int i, count;
	size_t len;
	caddr_t attr_val;
	crypto_object_attribute_t *k_attrs = NULL;

	ASSERT(in_key->ck_format == CRYPTO_KEY_ATTR_LIST);

	count = in_key->ck_count;
	/* figure out how much memory to allocate for everything */
	len = sizeof (crypto_key_t) +
	    count * sizeof (crypto_object_attribute_t);
	for (i = 0; i < count; i++) {
		len += roundup(in_key->ck_attrs[i].oa_value_len,
		    sizeof (caddr_t));
	}

	/* one big allocation for everything */
	ctx->key = kmem_alloc(len, kmflag);
	if (ctx->key == NULL)
		return (CRYPTO_HOST_MEMORY);
	k_attrs = (crypto_object_attribute_t *)((caddr_t)(ctx->key) +
	    sizeof (crypto_key_t));

	attr_val = (caddr_t)k_attrs +
	    count * sizeof (crypto_object_attribute_t);
	for (i = 0; i < count; i++) {
		k_attrs[i].oa_type = in_key->ck_attrs[i].oa_type;
		bcopy(in_key->ck_attrs[i].oa_value, attr_val,
		    in_key->ck_attrs[i].oa_value_len);
		k_attrs[i].oa_value = attr_val;
		k_attrs[i].oa_value_len = in_key->ck_attrs[i].oa_value_len;
		attr_val += roundup(k_attrs[i].oa_value_len, sizeof (caddr_t));
	}

	ctx->keychunk_size = len;	/* save the size to be freed */
	ctx->key->ck_format = CRYPTO_KEY_ATTR_LIST;
	ctx->key->ck_count = count;
	ctx->key->ck_attrs = k_attrs;

	return (CRYPTO_SUCCESS);
}

/* ARGSUSED */
static int
rsa_common_init(crypto_ctx_t *ctx, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_spi_ctx_template_t template,
    crypto_req_handle_t req)
{
	int rv;
	int kmflag;
	rsa_ctx_t *ctxp;

	if ((rv = check_mech_and_key(mechanism, key)) != CRYPTO_SUCCESS)
		return (rv);

	/*
	 * Allocate a RSA context.
	 */
	kmflag = crypto_kmflag(req);
	if ((ctxp = kmem_zalloc(sizeof (rsa_ctx_t), kmflag)) == NULL)
		return (CRYPTO_HOST_MEMORY);

	if ((rv = copy_key_to_ctx(key, ctxp, kmflag)) != CRYPTO_SUCCESS) {
		kmem_free(ctxp, sizeof (rsa_ctx_t));
		return (rv);
	}
	ctxp->mech_type = mechanism->cm_type;

	ctx->cc_provider_private = ctxp;

	return (CRYPTO_SUCCESS);
}

/* ARGSUSED */
static int
rsa_encrypt(crypto_ctx_t *ctx, crypto_data_t *plaintext,
    crypto_data_t *ciphertext, crypto_req_handle_t req)
{
	int rv;
	rsa_ctx_t *ctxp;

	ASSERT(ctx->cc_provider_private != NULL);
	ctxp = ctx->cc_provider_private;

	RSA_ARG_INPLACE(plaintext, ciphertext);

	/*
	 * Note on the KM_SLEEP flag passed to the routine below -
	 * rsa_encrypt() is a single-part encryption routine which is
	 * currently usable only by /dev/crypto. Since /dev/crypto calls are
	 * always synchronous, we can safely pass KM_SLEEP here.
	 */
	rv = rsa_encrypt_common(ctxp->mech_type, ctxp->key, plaintext,
	    ciphertext, KM_SLEEP);

	if (rv != CRYPTO_BUFFER_TOO_SMALL)
		(void) rsa_free_context(ctx);

	return (rv);
}

/* ARGSUSED */
static int
rsa_encrypt_atomic(crypto_provider_handle_t provider,
    crypto_session_id_t session_id, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_data_t *plaintext, crypto_data_t *ciphertext,
    crypto_spi_ctx_template_t template, crypto_req_handle_t req)
{
	int rv;

	if ((rv = check_mech_and_key(mechanism, key)) != CRYPTO_SUCCESS)
		return (rv);
	RSA_ARG_INPLACE(plaintext, ciphertext);

	return (rsa_encrypt_common(mechanism->cm_type, key, plaintext,
	    ciphertext, crypto_kmflag(req)));
}

static int
rsa_free_context(crypto_ctx_t *ctx)
{
	rsa_ctx_t *ctxp = ctx->cc_provider_private;

	if (ctxp != NULL) {
		bzero(ctxp->key, ctxp->keychunk_size);
		kmem_free(ctxp->key, ctxp->keychunk_size);

		if (ctxp->mech_type == RSA_PKCS_MECH_INFO_TYPE ||
		    ctxp->mech_type == RSA_X_509_MECH_INFO_TYPE)
			kmem_free(ctxp, sizeof (rsa_ctx_t));
		else
			kmem_free(ctxp, sizeof (digest_rsa_ctx_t));

		ctx->cc_provider_private = NULL;
	}

	return (CRYPTO_SUCCESS);
}

static int
rsa_encrypt_common(rsa_mech_type_t mech_type, crypto_key_t *key,
    crypto_data_t *plaintext, crypto_data_t *ciphertext, int kmflag)
{
	int rv = CRYPTO_FAILED;

/* EXPORT DELETE START */

	int plen;
	uchar_t *ptptr;
	uchar_t *modulus;
	ssize_t modulus_len;
	uchar_t tmp_data[MAX_RSA_KEYLENGTH_IN_BYTES];
	uchar_t plain_data[MAX_RSA_KEYLENGTH_IN_BYTES];
	uchar_t cipher_data[MAX_RSA_KEYLENGTH_IN_BYTES];

	if ((rv = get_key_attr(key, SUN_CKA_MODULUS, &modulus,
	    &modulus_len)) != CRYPTO_SUCCESS) {
		return (rv);
	}

	plen = plaintext->cd_length;
	if (mech_type == RSA_PKCS_MECH_INFO_TYPE) {
		if (plen > (modulus_len - MIN_PKCS1_PADLEN))
			return (CRYPTO_DATA_LEN_RANGE);
	} else {
		if (plen > modulus_len)
			return (CRYPTO_DATA_LEN_RANGE);
	}

	/*
	 * Output buf len must not be less than RSA modulus size.
	 */
	if (ciphertext->cd_length < modulus_len) {
		ciphertext->cd_length = modulus_len;
		return (CRYPTO_BUFFER_TOO_SMALL);
	}

	ASSERT(plaintext->cd_length <= sizeof (tmp_data));
	if ((rv = get_input_data(plaintext, &ptptr, tmp_data))
	    != CRYPTO_SUCCESS)
		return (rv);

	if (mech_type == RSA_PKCS_MECH_INFO_TYPE) {
		rv = soft_encrypt_rsa_pkcs_encode(ptptr, plen,
		    plain_data, modulus_len);

		if (rv != CRYPTO_SUCCESS)
			return (rv);
	} else {
		bzero(plain_data, modulus_len - plen);
		bcopy(ptptr, &plain_data[modulus_len - plen], plen);
	}

	rv = core_rsa_encrypt(key, plain_data, modulus_len,
	    cipher_data, kmflag, 1);
	if (rv == CRYPTO_SUCCESS) {
		/* copy out to ciphertext */
		if ((rv = put_output_data(cipher_data,
		    ciphertext, modulus_len)) != CRYPTO_SUCCESS)
			return (rv);

		ciphertext->cd_length = modulus_len;
	}

/* EXPORT DELETE END */

	return (rv);
}

/* EXPORT DELETE START */

static int
core_rsa_encrypt(crypto_key_t *key, uchar_t *in,
    int in_len, uchar_t *out, int kmflag, int is_public)
{
	int rv;
	uchar_t *expo, *modulus;
	ssize_t	expo_len;
	ssize_t modulus_len;
	BIGNUM msg;
	RSAkey *rsakey;

	if (is_public) {
		if ((rv = get_key_attr(key, SUN_CKA_PUBLIC_EXPONENT, &expo,
		    &expo_len)) != CRYPTO_SUCCESS)
			return (rv);
	} else {
		/*
		 * SUN_CKA_PRIVATE_EXPONENT is a required attribute for a
		 * RSA secret key. See the comments in core_rsa_decrypt
		 * routine which calls this routine with a private key.
		 */
		if ((rv = get_key_attr(key, SUN_CKA_PRIVATE_EXPONENT, &expo,
		    &expo_len)) != CRYPTO_SUCCESS)
			return (rv);
	}

	if ((rv = get_key_attr(key, SUN_CKA_MODULUS, &modulus,
	    &modulus_len)) != CRYPTO_SUCCESS) {
		return (rv);
	}

	rsakey = kmem_alloc(sizeof (RSAkey), kmflag);
	if (rsakey == NULL)
		return (CRYPTO_HOST_MEMORY);

	/* psize and qsize for RSA_key_init is in bits. */
	if (RSA_key_init(rsakey, modulus_len * 4, modulus_len * 4) != BIG_OK) {
		rv = CRYPTO_HOST_MEMORY;
		goto clean1;
	}

	/* Size for big_init is in (32-bit) words. */
	if (big_init(&msg, (in_len + (int)sizeof (uint32_t) - 1) /
	    (int)sizeof (uint32_t)) != BIG_OK) {
		rv = CRYPTO_HOST_MEMORY;
		goto clean2;
	}

	/* Convert octet string exponent to big integer format. */
	bytestring2bignum(&(rsakey->e), expo, expo_len);

	/* Convert octet string modulus to big integer format. */
	bytestring2bignum(&(rsakey->n), modulus, modulus_len);

	/* Convert octet string input data to big integer format. */
	bytestring2bignum(&msg, in, in_len);

	if (big_cmp_abs(&msg, &(rsakey->n)) > 0) {
		rv = CRYPTO_DATA_LEN_RANGE;
		goto clean3;
	}

	/* Perform RSA computation on big integer input data. */
	if (big_modexp(&msg, &msg, &(rsakey->e), &(rsakey->n), NULL)
	    != BIG_OK) {
		rv = CRYPTO_HOST_MEMORY;
		goto clean3;
	}

	/* Convert the big integer output data to octet string. */
	bignum2bytestring(out, &msg, modulus_len);

	/*
	 * Should not free modulus and expo as both are just pointers
	 * to an attribute value buffer from the caller.
	 */
clean3:
	big_finish(&msg);
clean2:
	RSA_key_finish(rsakey);
clean1:
	kmem_free(rsakey, sizeof (RSAkey));

	return (rv);
}

/* EXPORT DELETE END */

/* ARGSUSED */
static int
rsa_decrypt(crypto_ctx_t *ctx, crypto_data_t *ciphertext,
    crypto_data_t *plaintext, crypto_req_handle_t req)
{
	int rv;
	rsa_ctx_t *ctxp;

	ASSERT(ctx->cc_provider_private != NULL);
	ctxp = ctx->cc_provider_private;

	RSA_ARG_INPLACE(ciphertext, plaintext);

	/* See the comments on KM_SLEEP flag in rsa_encrypt() */
	rv = rsa_decrypt_common(ctxp->mech_type, ctxp->key,
	    ciphertext, plaintext, KM_SLEEP);

	if (rv != CRYPTO_BUFFER_TOO_SMALL)
		(void) rsa_free_context(ctx);

	return (rv);
}

/* ARGSUSED */
static int
rsa_decrypt_atomic(crypto_provider_handle_t provider,
    crypto_session_id_t session_id, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_data_t *ciphertext, crypto_data_t *plaintext,
    crypto_spi_ctx_template_t template, crypto_req_handle_t req)
{
	int rv;

	if ((rv = check_mech_and_key(mechanism, key)) != CRYPTO_SUCCESS)
		return (rv);
	RSA_ARG_INPLACE(ciphertext, plaintext);

	return (rsa_decrypt_common(mechanism->cm_type, key, ciphertext,
	    plaintext, crypto_kmflag(req)));
}

static int
rsa_decrypt_common(rsa_mech_type_t mech_type, crypto_key_t *key,
    crypto_data_t *ciphertext, crypto_data_t *plaintext, int kmflag)
{
	int rv = CRYPTO_FAILED;

/* EXPORT DELETE START */

	int plain_len;
	uchar_t *ctptr;
	uchar_t *modulus;
	ssize_t modulus_len;
	uchar_t plain_data[MAX_RSA_KEYLENGTH_IN_BYTES];
	uchar_t tmp_data[MAX_RSA_KEYLENGTH_IN_BYTES];

	if ((rv = get_key_attr(key, SUN_CKA_MODULUS, &modulus,
	    &modulus_len)) != CRYPTO_SUCCESS) {
		return (rv);
	}

	/*
	 * Ciphertext length must be equal to RSA modulus size.
	 */
	if (ciphertext->cd_length != modulus_len)
		return (CRYPTO_ENCRYPTED_DATA_LEN_RANGE);

	ASSERT(ciphertext->cd_length <= sizeof (tmp_data));
	if ((rv = get_input_data(ciphertext, &ctptr, tmp_data))
	    != CRYPTO_SUCCESS)
		return (rv);

	rv = core_rsa_decrypt(key, ctptr, modulus_len, plain_data, kmflag);
	if (rv == CRYPTO_SUCCESS) {
		plain_len = modulus_len;

		if (mech_type == RSA_PKCS_MECH_INFO_TYPE) {
			/* Strip off the PKCS block formatting data. */
			rv = soft_decrypt_rsa_pkcs_decode(plain_data,
			    &plain_len);
			if (rv != CRYPTO_SUCCESS)
				return (rv);
		}

		if (plain_len > plaintext->cd_length) {
			plaintext->cd_length = plain_len;
			return (CRYPTO_BUFFER_TOO_SMALL);
		}

		if ((rv = put_output_data(plain_data + modulus_len - plain_len,
		    plaintext, plain_len)) != CRYPTO_SUCCESS)
			return (rv);

		plaintext->cd_length = plain_len;
	}

/* EXPORT DELETE END */

	return (rv);
}

/* EXPORT DELETE START */

static int
core_rsa_decrypt(crypto_key_t *key, uchar_t *in, int in_len,
    uchar_t *out, int kmflag)
{
	int rv;
	uchar_t *modulus, *prime1, *prime2, *expo1, *expo2, *coef;
	ssize_t modulus_len;
	ssize_t	prime1_len, prime2_len;
	ssize_t	expo1_len, expo2_len, coef_len;
	BIGNUM msg;
	RSAkey *rsakey;

	if ((rv = get_key_attr(key, SUN_CKA_MODULUS, &modulus,
	    &modulus_len)) != CRYPTO_SUCCESS) {
		return (rv);
	}

	/*
	 * The following attributes are not required to be
	 * present in a RSA secret key. If any of them is not present
	 * we call the encrypt routine with a flag indicating use of
	 * private exponent (d). Note that SUN_CKA_PRIVATE_EXPONENT is
	 * a required attribute for a RSA secret key.
	 */
	if ((get_key_attr(key, SUN_CKA_PRIME_1, &prime1, &prime1_len)
		!= CRYPTO_SUCCESS) ||
	    (get_key_attr(key, SUN_CKA_PRIME_2, &prime2, &prime2_len)
		!= CRYPTO_SUCCESS) ||
	    (get_key_attr(key, SUN_CKA_EXPONENT_1, &expo1, &expo1_len)
		!= CRYPTO_SUCCESS) ||
	    (get_key_attr(key, SUN_CKA_EXPONENT_2, &expo2, &expo2_len)
		!= CRYPTO_SUCCESS) ||
	    (get_key_attr(key, SUN_CKA_COEFFICIENT, &coef, &coef_len)
		!= CRYPTO_SUCCESS)) {
		return (core_rsa_encrypt(key, in, in_len, out, kmflag, 0));
	}

	rsakey = kmem_alloc(sizeof (RSAkey), kmflag);
	if (rsakey == NULL)
		return (CRYPTO_HOST_MEMORY);

	/* psize and qsize for RSA_key_init is in bits. */
	if (RSA_key_init(rsakey, prime2_len * 8, prime1_len * 8) != BIG_OK) {
		rv = CRYPTO_HOST_MEMORY;
		goto clean1;
	}

	/* Size for big_init is in (32-bit) words. */
	if (big_init(&msg, (in_len + (int)sizeof (uint32_t) - 1) /
	    (int)sizeof (uint32_t)) != BIG_OK) {
		rv = CRYPTO_HOST_MEMORY;
		goto clean2;
	}

	/* Convert octet string input data to big integer format. */
	bytestring2bignum(&msg, in, in_len);

	/* Convert octet string modulus to big integer format. */
	bytestring2bignum(&(rsakey->n), modulus, modulus_len);

	if (big_cmp_abs(&msg, &(rsakey->n)) > 0) {
		rv = CRYPTO_DATA_LEN_RANGE;
		goto clean3;
	}

	/* Convert the rest of private key attributes to big integer format. */
	bytestring2bignum(&(rsakey->dmodpminus1), expo2, expo2_len);
	bytestring2bignum(&(rsakey->dmodqminus1), expo1, expo1_len);
	bytestring2bignum(&(rsakey->p), prime2, prime2_len);
	bytestring2bignum(&(rsakey->q), prime1, prime1_len);
	bytestring2bignum(&(rsakey->pinvmodq), coef, coef_len);

	if ((big_cmp_abs(&(rsakey->dmodpminus1), &(rsakey->p)) > 0) ||
	    (big_cmp_abs(&(rsakey->dmodqminus1), &(rsakey->q)) > 0) ||
	    (big_cmp_abs(&(rsakey->pinvmodq), &(rsakey->q)) > 0)) {
		rv = CRYPTO_KEY_SIZE_RANGE;
		goto clean3;
	}

	/* Perform RSA computation on big integer input data. */
	if (big_modexp_crt(&msg, &msg, &(rsakey->dmodpminus1),
	    &(rsakey->dmodqminus1), &(rsakey->p), &(rsakey->q),
	    &(rsakey->pinvmodq), NULL, NULL) != BIG_OK) {
		rv = CRYPTO_HOST_MEMORY;
		goto clean3;
	}

	/* Convert the big integer output data to octet string. */
	bignum2bytestring(out, &msg, modulus_len);

	/*
	 * Should not free modulus and friends as they are just pointers
	 * to an attribute value buffer from the caller.
	 */
clean3:
	big_finish(&msg);
clean2:
	RSA_key_finish(rsakey);
clean1:
	kmem_free(rsakey, sizeof (RSAkey));

	return (rv);
}

/* EXPORT DELETE END */

/* ARGSUSED */
static int
rsa_sign_verify_common_init(crypto_ctx_t *ctx, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_spi_ctx_template_t ctx_template,
    crypto_req_handle_t req)
{
	int rv;
	int kmflag;
	rsa_ctx_t *ctxp;
	digest_rsa_ctx_t *dctxp;

	if ((rv = check_mech_and_key(mechanism, key)) != CRYPTO_SUCCESS)
		return (rv);

	/*
	 * Allocate a RSA context.
	 */
	kmflag = crypto_kmflag(req);
	switch (mechanism->cm_type) {
	case MD5_RSA_PKCS_MECH_INFO_TYPE:
	case SHA1_RSA_PKCS_MECH_INFO_TYPE:
	case SHA256_RSA_PKCS_MECH_INFO_TYPE:
	case SHA384_RSA_PKCS_MECH_INFO_TYPE:
	case SHA512_RSA_PKCS_MECH_INFO_TYPE:
		dctxp = kmem_zalloc(sizeof (digest_rsa_ctx_t), kmflag);
		ctxp = (rsa_ctx_t *)dctxp;
		break;
	default:
		ctxp = kmem_zalloc(sizeof (rsa_ctx_t), kmflag);
		break;
	}

	if (ctxp == NULL)
		return (CRYPTO_HOST_MEMORY);

	ctxp->mech_type = mechanism->cm_type;
	if ((rv = copy_key_to_ctx(key, ctxp, kmflag)) != CRYPTO_SUCCESS) {
		switch (mechanism->cm_type) {
		case MD5_RSA_PKCS_MECH_INFO_TYPE:
		case SHA1_RSA_PKCS_MECH_INFO_TYPE:
		case SHA256_RSA_PKCS_MECH_INFO_TYPE:
		case SHA384_RSA_PKCS_MECH_INFO_TYPE:
		case SHA512_RSA_PKCS_MECH_INFO_TYPE:
			kmem_free(dctxp, sizeof (digest_rsa_ctx_t));
			break;
		default:
			kmem_free(ctxp, sizeof (rsa_ctx_t));
			break;
		}
		return (rv);
	}

	switch (mechanism->cm_type) {
	case MD5_RSA_PKCS_MECH_INFO_TYPE:
		MD5Init(&(dctxp->md5_ctx));
		break;

	case SHA1_RSA_PKCS_MECH_INFO_TYPE:
		SHA1Init(&(dctxp->sha1_ctx));
		break;

	case SHA256_RSA_PKCS_MECH_INFO_TYPE:
		SHA2Init(SHA256, &(dctxp->sha2_ctx));
		break;

	case SHA384_RSA_PKCS_MECH_INFO_TYPE:
		SHA2Init(SHA384, &(dctxp->sha2_ctx));
		break;

	case SHA512_RSA_PKCS_MECH_INFO_TYPE:
		SHA2Init(SHA512, &(dctxp->sha2_ctx));
		break;
	}

	ctx->cc_provider_private = ctxp;

	return (CRYPTO_SUCCESS);
}

#define	SHA1_DIGEST_SIZE 20
#define	MD5_DIGEST_SIZE 16

#define	INIT_RAW_CRYPTO_DATA(data, base, len, cd_len)	\
	(data).cd_format = CRYPTO_DATA_RAW;		\
	(data).cd_offset = 0;				\
	(data).cd_raw.iov_base = (char *)base;		\
	(data).cd_raw.iov_len = len;			\
	(data).cd_length = cd_len;

#define	DO_UPDATE	0x01
#define	DO_FINAL	0x02
#define	DO_MD5		0x04
#define	DO_SHA1		0x08
#define	DO_SIGN		0x10
#define	DO_VERIFY	0x20
#define	DO_SHA2		0x40

static int
digest_data(crypto_data_t *data, void *dctx, uchar_t *digest,
    uchar_t flag)
{
	int rv, dlen;
	uchar_t *dptr;

	ASSERT(flag & DO_MD5 || flag & DO_SHA1 || flag & DO_SHA2);
	if (data == NULL) {
		ASSERT((flag & DO_UPDATE) == 0);
		goto dofinal;
	}

	dlen = data->cd_length;

	if (flag & DO_UPDATE) {

		switch (data->cd_format) {
		case CRYPTO_DATA_RAW:
			dptr = (uchar_t *)(data->cd_raw.iov_base +
			    data->cd_offset);

			if (flag & DO_MD5)
				MD5Update(dctx, dptr, dlen);

			else if (flag & DO_SHA1)
				SHA1Update(dctx, dptr, dlen);

			else
				SHA2Update(dctx, dptr, dlen);

		break;

		case CRYPTO_DATA_UIO:
			if (flag & DO_MD5)
				rv = process_uio_data(data, NULL, dlen,
				    MD5_DIGEST_DATA, dctx);

			else if (flag & DO_SHA1)
				rv = process_uio_data(data, NULL, dlen,
				    SHA1_DIGEST_DATA, dctx);

			else
				rv = process_uio_data(data, NULL, dlen,
				    SHA2_DIGEST_DATA, dctx);

			if (rv != CRYPTO_SUCCESS)
				return (rv);

			break;

		case CRYPTO_DATA_MBLK:
			if (flag & DO_MD5)
				rv = process_mblk_data(data, NULL, dlen,
				    MD5_DIGEST_DATA, dctx);

			else if (flag & DO_SHA1)
				rv = process_mblk_data(data, NULL, dlen,
				    SHA1_DIGEST_DATA, dctx);

			else
				rv = process_mblk_data(data, NULL, dlen,
				    SHA2_DIGEST_DATA, dctx);

			if (rv != CRYPTO_SUCCESS)
				return (rv);

			break;
		}
	}

dofinal:
	if (flag & DO_FINAL) {
		if (flag & DO_MD5)
			MD5Final(digest, dctx);

		else if (flag & DO_SHA1)
			SHA1Final(digest, dctx);

		else
			SHA2Final(digest, dctx);
	}

	return (CRYPTO_SUCCESS);
}

static int
rsa_digest_svrfy_common(digest_rsa_ctx_t *ctxp, crypto_data_t *data,
    crypto_data_t *signature, int kmflag, uchar_t flag)
{
	int rv = CRYPTO_FAILED;

/* EXPORT DELETE START */

	uchar_t digest[SHA512_DIGEST_LENGTH];
	/* The der_data size is enough for MD5 also */
	uchar_t der_data[SHA512_DIGEST_LENGTH + SHA2_DER_PREFIX_Len];
	ulong_t der_data_len;
	crypto_data_t der_cd;
	rsa_mech_type_t mech_type;

	ASSERT(flag & DO_SIGN || flag & DO_VERIFY);
	ASSERT(data != NULL || (flag & DO_FINAL));

	mech_type = ctxp->mech_type;
	if (mech_type == RSA_PKCS_MECH_INFO_TYPE ||
	    mech_type == RSA_X_509_MECH_INFO_TYPE)
		return (CRYPTO_MECHANISM_INVALID);

	/*
	 * We need to do the BUFFER_TOO_SMALL check before digesting
	 * the data. No check is needed for verify as signature is not
	 * an output argument for verify.
	 */
	if (flag & DO_SIGN) {
		uchar_t *modulus;
		ssize_t modulus_len;

		if ((rv = get_key_attr(ctxp->key, SUN_CKA_MODULUS, &modulus,
		    &modulus_len)) != CRYPTO_SUCCESS) {
			return (rv);
		}

		if (signature->cd_length < modulus_len) {
			signature->cd_length = modulus_len;
			return (CRYPTO_BUFFER_TOO_SMALL);
		}
	}

	if (mech_type == MD5_RSA_PKCS_MECH_INFO_TYPE)
		rv = digest_data(data, &(ctxp->md5_ctx),
		    digest, flag | DO_MD5);

	else if (mech_type == SHA1_RSA_PKCS_MECH_INFO_TYPE)
		rv = digest_data(data, &(ctxp->sha1_ctx),
		    digest, flag | DO_SHA1);

	else
		rv = digest_data(data, &(ctxp->sha2_ctx),
		    digest, flag | DO_SHA2);

	if (rv != CRYPTO_SUCCESS)
		return (rv);


	/*
	 * Prepare the DER encoding of the DigestInfo value as follows:
	 * MD5:		MD5_DER_PREFIX || H
	 * SHA-1:	SHA1_DER_PREFIX || H
	 *
	 * See rsa_impl.c for more details.
	 */
	switch (mech_type) {
	case MD5_RSA_PKCS_MECH_INFO_TYPE:
		bcopy(MD5_DER_PREFIX, der_data, MD5_DER_PREFIX_Len);
		bcopy(digest, der_data + MD5_DER_PREFIX_Len, MD5_DIGEST_SIZE);
		der_data_len = MD5_DER_PREFIX_Len + MD5_DIGEST_SIZE;
		break;

	case SHA1_RSA_PKCS_MECH_INFO_TYPE:
		bcopy(SHA1_DER_PREFIX, der_data, SHA1_DER_PREFIX_Len);
		bcopy(digest, der_data + SHA1_DER_PREFIX_Len,
		    SHA1_DIGEST_SIZE);
		der_data_len = SHA1_DER_PREFIX_Len + SHA1_DIGEST_SIZE;
		break;

	case SHA256_RSA_PKCS_MECH_INFO_TYPE:
		bcopy(SHA256_DER_PREFIX, der_data, SHA2_DER_PREFIX_Len);
		bcopy(digest, der_data + SHA2_DER_PREFIX_Len,
		    SHA256_DIGEST_LENGTH);
		der_data_len = SHA2_DER_PREFIX_Len + SHA256_DIGEST_LENGTH;
		break;

	case SHA384_RSA_PKCS_MECH_INFO_TYPE:
		bcopy(SHA384_DER_PREFIX, der_data, SHA2_DER_PREFIX_Len);
		bcopy(digest, der_data + SHA2_DER_PREFIX_Len,
		    SHA384_DIGEST_LENGTH);
		der_data_len = SHA2_DER_PREFIX_Len + SHA384_DIGEST_LENGTH;
		break;

	case SHA512_RSA_PKCS_MECH_INFO_TYPE:
		bcopy(SHA512_DER_PREFIX, der_data, SHA2_DER_PREFIX_Len);
		bcopy(digest, der_data + SHA2_DER_PREFIX_Len,
		    SHA512_DIGEST_LENGTH);
		der_data_len = SHA2_DER_PREFIX_Len + SHA512_DIGEST_LENGTH;
		break;
	}

	INIT_RAW_CRYPTO_DATA(der_cd, der_data, der_data_len, der_data_len);
	/*
	 * Now, we are ready to sign or verify the DER_ENCODED data.
	 */
	if (flag & DO_SIGN)
		rv = rsa_sign_common(mech_type, ctxp->key, &der_cd,
		    signature, kmflag);
	else
		rv = rsa_verify_common(mech_type, ctxp->key, &der_cd,
		    signature, kmflag);

/* EXPORT DELETE END */

	return (rv);
}

static int
rsa_sign_common(rsa_mech_type_t mech_type, crypto_key_t *key,
    crypto_data_t *data, crypto_data_t *signature, int kmflag)
{
	int rv = CRYPTO_FAILED;

/* EXPORT DELETE START */

	int dlen;
	uchar_t *dataptr, *modulus;
	ssize_t modulus_len;
	uchar_t tmp_data[MAX_RSA_KEYLENGTH_IN_BYTES];
	uchar_t plain_data[MAX_RSA_KEYLENGTH_IN_BYTES];
	uchar_t signed_data[MAX_RSA_KEYLENGTH_IN_BYTES];

	if ((rv = get_key_attr(key, SUN_CKA_MODULUS, &modulus,
	    &modulus_len)) != CRYPTO_SUCCESS) {
		return (rv);
	}

	dlen = data->cd_length;
	switch (mech_type) {
	case RSA_PKCS_MECH_INFO_TYPE:
		if (dlen > (modulus_len - MIN_PKCS1_PADLEN))
			return (CRYPTO_DATA_LEN_RANGE);
		break;
	case RSA_X_509_MECH_INFO_TYPE:
		if (dlen > modulus_len)
			return (CRYPTO_DATA_LEN_RANGE);
		break;
	}

	if (signature->cd_length < modulus_len) {
		signature->cd_length = modulus_len;
		return (CRYPTO_BUFFER_TOO_SMALL);
	}

	ASSERT(data->cd_length <= sizeof (tmp_data));
	if ((rv = get_input_data(data, &dataptr, tmp_data))
	    != CRYPTO_SUCCESS)
		return (rv);

	switch (mech_type) {
	case RSA_PKCS_MECH_INFO_TYPE:
	case MD5_RSA_PKCS_MECH_INFO_TYPE:
	case SHA1_RSA_PKCS_MECH_INFO_TYPE:
	case SHA256_RSA_PKCS_MECH_INFO_TYPE:
	case SHA384_RSA_PKCS_MECH_INFO_TYPE:
	case SHA512_RSA_PKCS_MECH_INFO_TYPE:
		/*
		 * Add PKCS padding to the input data to format a block
		 * type "01" encryption block.
		 */
		rv = soft_sign_rsa_pkcs_encode(dataptr, dlen, plain_data,
		    modulus_len);
		if (rv != CRYPTO_SUCCESS)
			return (rv);

		break;

	case RSA_X_509_MECH_INFO_TYPE:
		bzero(plain_data, modulus_len - dlen);
		bcopy(dataptr, &plain_data[modulus_len - dlen], dlen);
		break;
	}

	rv = core_rsa_decrypt(key, plain_data, modulus_len, signed_data,
	    kmflag);
	if (rv == CRYPTO_SUCCESS) {
		/* copy out to signature */
		if ((rv = put_output_data(signed_data,
		    signature, modulus_len)) != CRYPTO_SUCCESS)
			return (rv);

		signature->cd_length = modulus_len;
	}

/* EXPORT DELETE END */

	return (rv);
}

/* ARGSUSED */
static int
rsa_sign(crypto_ctx_t *ctx, crypto_data_t *data, crypto_data_t *signature,
    crypto_req_handle_t req)
{
	int rv;
	rsa_ctx_t *ctxp;

	ASSERT(ctx->cc_provider_private != NULL);
	ctxp = ctx->cc_provider_private;

	/* See the comments on KM_SLEEP flag in rsa_encrypt() */
	switch (ctxp->mech_type) {
	case MD5_RSA_PKCS_MECH_INFO_TYPE:
	case SHA1_RSA_PKCS_MECH_INFO_TYPE:
	case SHA256_RSA_PKCS_MECH_INFO_TYPE:
	case SHA384_RSA_PKCS_MECH_INFO_TYPE:
	case SHA512_RSA_PKCS_MECH_INFO_TYPE:
		rv = rsa_digest_svrfy_common((digest_rsa_ctx_t *)ctxp, data,
		    signature, KM_SLEEP, DO_SIGN | DO_UPDATE | DO_FINAL);
		break;
	default:
		rv = rsa_sign_common(ctxp->mech_type, ctxp->key, data,
		    signature, KM_SLEEP);
		break;
	}

	if (rv != CRYPTO_BUFFER_TOO_SMALL)
		(void) rsa_free_context(ctx);

	return (rv);
}

/* ARGSUSED */
static int
rsa_sign_update(crypto_ctx_t *ctx, crypto_data_t *data, crypto_req_handle_t req)
{
	int rv;
	digest_rsa_ctx_t *ctxp;
	rsa_mech_type_t mech_type;

	ASSERT(ctx->cc_provider_private != NULL);
	ctxp = ctx->cc_provider_private;
	mech_type = ctxp->mech_type;

	if (mech_type == RSA_PKCS_MECH_INFO_TYPE ||
	    mech_type == RSA_X_509_MECH_INFO_TYPE)
		return (CRYPTO_MECHANISM_INVALID);

	if (mech_type == MD5_RSA_PKCS_MECH_INFO_TYPE)
		rv = digest_data(data, &(ctxp->md5_ctx),
		    NULL, DO_MD5 | DO_UPDATE);

	else if (mech_type == SHA1_RSA_PKCS_MECH_INFO_TYPE)
		rv = digest_data(data, &(ctxp->sha1_ctx),
		    NULL, DO_SHA1 | DO_UPDATE);

	else
		rv = digest_data(data, &(ctxp->sha2_ctx),
		    NULL, DO_SHA2 | DO_UPDATE);

	return (rv);
}

static int
rsa_sign_final(crypto_ctx_t *ctx, crypto_data_t *signature,
    crypto_req_handle_t req)
{
	int rv;
	digest_rsa_ctx_t *ctxp;

	ASSERT(ctx->cc_provider_private != NULL);
	ctxp = ctx->cc_provider_private;

	rv = rsa_digest_svrfy_common(ctxp, NULL, signature,
	    crypto_kmflag(req), DO_SIGN | DO_FINAL);
	if (rv != CRYPTO_BUFFER_TOO_SMALL)
		(void) rsa_free_context(ctx);

	return (rv);
}

/* ARGSUSED */
static int
rsa_sign_atomic(crypto_provider_handle_t provider,
    crypto_session_id_t session_id, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_data_t *data, crypto_data_t *signature,
    crypto_spi_ctx_template_t ctx_template, crypto_req_handle_t req)
{
	int rv;
	digest_rsa_ctx_t dctx;

	if ((rv = check_mech_and_key(mechanism, key)) != CRYPTO_SUCCESS)
		return (rv);

	if (mechanism->cm_type == RSA_PKCS_MECH_INFO_TYPE ||
	    mechanism->cm_type == RSA_X_509_MECH_INFO_TYPE)
		rv = rsa_sign_common(mechanism->cm_type, key, data,
		    signature, crypto_kmflag(req));

	else {
		dctx.mech_type = mechanism->cm_type;
		dctx.key = key;
		switch (mechanism->cm_type) {
		case MD5_RSA_PKCS_MECH_INFO_TYPE:
			MD5Init(&(dctx.md5_ctx));
			break;

		case SHA1_RSA_PKCS_MECH_INFO_TYPE:
			SHA1Init(&(dctx.sha1_ctx));
			break;

		case SHA256_RSA_PKCS_MECH_INFO_TYPE:
			SHA2Init(SHA256, &(dctx.sha2_ctx));
			break;

		case SHA384_RSA_PKCS_MECH_INFO_TYPE:
			SHA2Init(SHA384, &(dctx.sha2_ctx));
			break;

		case SHA512_RSA_PKCS_MECH_INFO_TYPE:
			SHA2Init(SHA512, &(dctx.sha2_ctx));
			break;
		}

		rv = rsa_digest_svrfy_common(&dctx, data, signature,
		    crypto_kmflag(req), DO_SIGN | DO_UPDATE | DO_FINAL);
	}

	return (rv);
}

static int
rsa_verify_common(rsa_mech_type_t mech_type, crypto_key_t *key,
    crypto_data_t *data, crypto_data_t *signature, int kmflag)
{
	int rv = CRYPTO_FAILED;

/* EXPORT DELETE START */

	uchar_t *sigptr, *modulus;
	ssize_t modulus_len;
	uchar_t plain_data[MAX_RSA_KEYLENGTH_IN_BYTES];
	uchar_t tmp_data[MAX_RSA_KEYLENGTH_IN_BYTES];

	if ((rv = get_key_attr(key, SUN_CKA_MODULUS, &modulus,
	    &modulus_len)) != CRYPTO_SUCCESS) {
		return (rv);
	}

	if (signature->cd_length != modulus_len)
		return (CRYPTO_SIGNATURE_LEN_RANGE);

	ASSERT(signature->cd_length <= sizeof (tmp_data));
	if ((rv = get_input_data(signature, &sigptr, tmp_data))
	    != CRYPTO_SUCCESS)
		return (rv);

	rv = core_rsa_encrypt(key, sigptr, modulus_len, plain_data, kmflag, 1);
	if (rv != CRYPTO_SUCCESS)
		return (rv);

	if (mech_type == RSA_X_509_MECH_INFO_TYPE) {
		if (compare_data(data, (plain_data + modulus_len
		    - data->cd_length)) != 0)
			rv = CRYPTO_SIGNATURE_INVALID;

	} else {
		int data_len = modulus_len;

		/*
		 * Strip off the encoded padding bytes in front of the
		 * recovered data, then compare the recovered data with
		 * the original data.
		 */
		rv = soft_verify_rsa_pkcs_decode(plain_data, &data_len);
		if (rv != CRYPTO_SUCCESS)
			return (rv);

		if (data_len != data->cd_length)
			return (CRYPTO_SIGNATURE_LEN_RANGE);

		if (compare_data(data, (plain_data + modulus_len
		    - data_len)) != 0)
			rv = CRYPTO_SIGNATURE_INVALID;
	}

/* EXPORT DELETE END */

	return (rv);
}

/* ARGSUSED */
static int
rsa_verify(crypto_ctx_t *ctx, crypto_data_t *data, crypto_data_t *signature,
    crypto_req_handle_t req)
{
	int rv;
	rsa_ctx_t *ctxp;

	ASSERT(ctx->cc_provider_private != NULL);
	ctxp = ctx->cc_provider_private;

	/* See the comments on KM_SLEEP flag in rsa_encrypt() */
	switch (ctxp->mech_type) {
	case MD5_RSA_PKCS_MECH_INFO_TYPE:
	case SHA1_RSA_PKCS_MECH_INFO_TYPE:
	case SHA256_RSA_PKCS_MECH_INFO_TYPE:
	case SHA384_RSA_PKCS_MECH_INFO_TYPE:
	case SHA512_RSA_PKCS_MECH_INFO_TYPE:
		rv = rsa_digest_svrfy_common((digest_rsa_ctx_t *)ctxp, data,
		    signature, KM_SLEEP, DO_VERIFY | DO_UPDATE | DO_FINAL);
		break;
	default:
		rv = rsa_verify_common(ctxp->mech_type, ctxp->key, data,
		    signature, KM_SLEEP);
		break;
	}

	if (rv != CRYPTO_BUFFER_TOO_SMALL)
		(void) rsa_free_context(ctx);

	return (rv);
}

/* ARGSUSED */
static int
rsa_verify_update(crypto_ctx_t *ctx, crypto_data_t *data,
    crypto_req_handle_t req)
{
	int rv;
	digest_rsa_ctx_t *ctxp;

	ASSERT(ctx->cc_provider_private != NULL);
	ctxp = ctx->cc_provider_private;

	switch (ctxp->mech_type) {

	case MD5_RSA_PKCS_MECH_INFO_TYPE:
		rv = digest_data(data, &(ctxp->md5_ctx),
		    NULL, DO_MD5 | DO_UPDATE);
		break;

	case SHA1_RSA_PKCS_MECH_INFO_TYPE:
		rv = digest_data(data, &(ctxp->sha1_ctx),
		    NULL, DO_SHA1 | DO_UPDATE);
		break;

	case SHA256_RSA_PKCS_MECH_INFO_TYPE:
	case SHA384_RSA_PKCS_MECH_INFO_TYPE:
	case SHA512_RSA_PKCS_MECH_INFO_TYPE:
		rv = digest_data(data, &(ctxp->sha2_ctx),
		    NULL, DO_SHA2 | DO_UPDATE);
		break;

	default:
		return (CRYPTO_MECHANISM_INVALID);
	}

	return (rv);
}

static int
rsa_verify_final(crypto_ctx_t *ctx, crypto_data_t *signature,
    crypto_req_handle_t req)
{
	int rv;
	digest_rsa_ctx_t *ctxp;

	ASSERT(ctx->cc_provider_private != NULL);
	ctxp = ctx->cc_provider_private;

	rv = rsa_digest_svrfy_common(ctxp, NULL, signature,
	    crypto_kmflag(req), DO_VERIFY | DO_FINAL);
	if (rv != CRYPTO_BUFFER_TOO_SMALL)
		(void) rsa_free_context(ctx);

	return (rv);
}


/* ARGSUSED */
static int
rsa_verify_atomic(crypto_provider_handle_t provider,
    crypto_session_id_t session_id,
    crypto_mechanism_t *mechanism, crypto_key_t *key, crypto_data_t *data,
    crypto_data_t *signature, crypto_spi_ctx_template_t ctx_template,
    crypto_req_handle_t req)
{
	int rv;
	digest_rsa_ctx_t dctx;

	if ((rv = check_mech_and_key(mechanism, key)) != CRYPTO_SUCCESS)
		return (rv);

	if (mechanism->cm_type == RSA_PKCS_MECH_INFO_TYPE ||
	    mechanism->cm_type == RSA_X_509_MECH_INFO_TYPE)
		rv = rsa_verify_common(mechanism->cm_type, key, data,
		    signature, crypto_kmflag(req));

	else {
		dctx.mech_type = mechanism->cm_type;
		dctx.key = key;

		switch (mechanism->cm_type) {
		case MD5_RSA_PKCS_MECH_INFO_TYPE:
			MD5Init(&(dctx.md5_ctx));
			break;

		case SHA1_RSA_PKCS_MECH_INFO_TYPE:
			SHA1Init(&(dctx.sha1_ctx));
			break;

		case SHA256_RSA_PKCS_MECH_INFO_TYPE:
			SHA2Init(SHA256, &(dctx.sha2_ctx));
			break;

		case SHA384_RSA_PKCS_MECH_INFO_TYPE:
			SHA2Init(SHA384, &(dctx.sha2_ctx));
			break;

		case SHA512_RSA_PKCS_MECH_INFO_TYPE:
			SHA2Init(SHA512, &(dctx.sha2_ctx));
			break;
		}

		rv = rsa_digest_svrfy_common(&dctx, data,
		    signature, crypto_kmflag(req),
		    DO_VERIFY | DO_UPDATE | DO_FINAL);
	}

	return (rv);
}

static int
rsa_verify_recover_common(rsa_mech_type_t mech_type, crypto_key_t *key,
    crypto_data_t *signature, crypto_data_t *data, int kmflag)
{
	int rv = CRYPTO_FAILED;

/* EXPORT DELETE START */

	int data_len;
	uchar_t *sigptr, *modulus;
	ssize_t modulus_len;
	uchar_t plain_data[MAX_RSA_KEYLENGTH_IN_BYTES];
	uchar_t tmp_data[MAX_RSA_KEYLENGTH_IN_BYTES];

	if ((rv = get_key_attr(key, SUN_CKA_MODULUS, &modulus,
	    &modulus_len)) != CRYPTO_SUCCESS) {
		return (rv);
	}

	if (signature->cd_length != modulus_len)
		return (CRYPTO_SIGNATURE_LEN_RANGE);

	ASSERT(signature->cd_length <= sizeof (tmp_data));
	if ((rv = get_input_data(signature, &sigptr, tmp_data))
	    != CRYPTO_SUCCESS)
		return (rv);

	rv = core_rsa_encrypt(key, sigptr, modulus_len, plain_data, kmflag, 1);
	if (rv != CRYPTO_SUCCESS)
		return (rv);

	data_len = modulus_len;

	if (mech_type == RSA_PKCS_MECH_INFO_TYPE) {
		/*
		 * Strip off the encoded padding bytes in front of the
		 * recovered data, then compare the recovered data with
		 * the original data.
		 */
		rv = soft_verify_rsa_pkcs_decode(plain_data, &data_len);
		if (rv != CRYPTO_SUCCESS)
			return (rv);
	}

	if (data->cd_length < data_len) {
		data->cd_length = data_len;
		return (CRYPTO_BUFFER_TOO_SMALL);
	}

	if ((rv = put_output_data(plain_data + modulus_len - data_len,
	    data, data_len)) != CRYPTO_SUCCESS)
		return (rv);
	data->cd_length = data_len;

/* EXPORT DELETE END */

	return (rv);
}

/* ARGSUSED */
static int
rsa_verify_recover(crypto_ctx_t *ctx, crypto_data_t *signature,
    crypto_data_t *data, crypto_req_handle_t req)
{
	int rv;
	rsa_ctx_t *ctxp;

	ASSERT(ctx->cc_provider_private != NULL);
	ctxp = ctx->cc_provider_private;

	/* See the comments on KM_SLEEP flag in rsa_encrypt() */
	rv = rsa_verify_recover_common(ctxp->mech_type, ctxp->key,
	    signature, data, KM_SLEEP);

	if (rv != CRYPTO_BUFFER_TOO_SMALL)
		(void) rsa_free_context(ctx);

	return (rv);
}

/* ARGSUSED */
static int
rsa_verify_recover_atomic(crypto_provider_handle_t provider,
    crypto_session_id_t session_id, crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_data_t *signature, crypto_data_t *data,
    crypto_spi_ctx_template_t ctx_template, crypto_req_handle_t req)
{
	int rv;

	if ((rv = check_mech_and_key(mechanism, key)) != CRYPTO_SUCCESS)
		return (rv);

	return (rsa_verify_recover_common(mechanism->cm_type, key,
	    signature, data, crypto_kmflag(req)));
}
