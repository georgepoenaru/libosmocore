# This is _NOT_ the library release version, it's an API version.
# Please read chapter "Library interface versions" of the libtool documentation
# before making any modifications: https://www.gnu.org/software/libtool/manual/html_node/Versioning.html
LIBVERSION=10:0:0

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include $(TALLOC_CFLAGS)
AM_CFLAGS = -Wall ${GCC_FVISIBILITY_HIDDEN}

if ENABLE_PSEUDOTALLOC
AM_CPPFLAGS += -I$(top_srcdir)/src/pseudotalloc
endif

# FIXME: this should eventually go into a milenage/Makefile.am
noinst_HEADERS = milenage/aes.h milenage/aes_i.h milenage/aes_wrap.h \
		 milenage/common.h milenage/crypto.h milenage/includes.h \
		 milenage/milenage.h

noinst_LTLIBRARIES = libgsmint.la
lib_LTLIBRARIES = libosmogsm.la

BUILT_SOURCES = gsm0503_conv.c

libgsmint_la_SOURCES =  a5.c rxlev_stat.c tlv_parser.c comp128.c comp128v23.c \
			gsm_utils.c rsl.c gsm48.c gsm48_ie.c gsm0808.c sysinfo.c \
			gprs_cipher_core.c gprs_rlc.c gsm0480.c abis_nm.c gsm0502.c \
			gsm0411_utils.c gsm0411_smc.c gsm0411_smr.c gsm0414.c \
			lapd_core.c lapdm.c kasumi.c gsm29205.c gsm_04_08_gprs.c \
			auth_core.c auth_comp128v1.c auth_comp128v23.c \
			auth_milenage.c milenage/aes-encblock.c gea.c \
			milenage/aes-internal.c milenage/aes-internal-enc.c \
			milenage/milenage.c gan.c ipa.c gsm0341.c apn.c \
			gsup.c gsup_sms.c gprs_gea.c gsm0503_conv.c oap.c gsm0808_utils.c \
			gsm23003.c mncc.c bts_features.c oap_client.c \
			gsm29118.c
libgsmint_la_LDFLAGS = -no-undefined
libgsmint_la_LIBADD = $(top_builddir)/src/libosmocore.la

libosmogsm_la_SOURCES =
libosmogsm_la_LDFLAGS = $(LTLDFLAGS_OSMOGSM) -version-info $(LIBVERSION) -no-undefined
libosmogsm_la_LIBADD = libgsmint.la $(TALLOC_LIBS)

if ENABLE_GNUTLS
AM_CPPFLAGS += $(LIBGNUTLS_CFLAGS)
libosmogsm_la_LIBADD += $(LIBGNUTLS_LIBS)
endif

EXTRA_DIST = libosmogsm.map

# Convolutional codes generation
gsm0503_conv.c: $(top_srcdir)/utils/conv_gen.py $(top_srcdir)/utils/conv_codes_gsm.py
	$(AM_V_GEN)python $(top_srcdir)/utils/conv_gen.py gen_codes gsm

CLEANFILES = gsm0503_conv.c
