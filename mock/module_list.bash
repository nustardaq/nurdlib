#!/bin/bash

# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2025
# Hans Toshihide Törnqvist
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301  USA

cd $(dirname $0)

jobs=-j8
[ 1 -eq $# ] && jobs=$1

for m in caen_v1190 caen_v1290 \
	    caen_v767a \
	    caen_v775 caen_v785 caen_v785n caen_v792 caen_v965 \
	    caen_v560 caen_v820 caen_v830 caen_v895 \
	    caen_v1725 \
	    gsi_ctdc gsi_febex gsi_kilom \
	    gsi_mppc_rob gsi_pex gsi_pexaria gsi_sam gsi_siderem \
	    gsi_tacquila gsi_tamex gsi_tridi gsi_triva gsi_vetar gsi_vftx2 \
	    gsi_vulom gsi_vuprom gsi_rfx1 \
	    mesytec_madc32 mesytec_mqdc32 mesytec_mtdc32 \
	    mesytec_mdpp16scp mesytec_mdpp16qdc \
	    mesytec_mdpp32qdc mesytec_mdpp32scp \
	    mesytec_vmmr8 \
	    pnpi_cros3 \
	    sis_3316 sis_3801 sis_3820 sis_3820_scaler
do
	n=$m
	[ $m = caen_v1190 ] && n="$n caen_v1n90"
	[ $m = caen_v1290 ] && n="$n caen_v1n90"
	[ $m = caen_v775 ] && n="$n caen_v7nn"
	[ $m = caen_v785 ] && n="$n caen_v7nn"
	[ $m = caen_v785n ] && n="$n caen_v7nn"
	[ $m = caen_v792 ] && n="$n caen_v7nn"
	[ $m = caen_v965 ] && n="$n caen_v7nn"
	[ $m = caen_v820 ] && n="$n caen_v8n0"
	[ $m = caen_v830 ] && n="$n caen_v8n0"
	[ $m = gsi_ctdc ] && n="$n gsi_ctdc_proto gsi_pex"
	[ $m = gsi_febex ] && n="$n gsi_pex"
	[ $m = gsi_kilom ] && n="$n gsi_pex"
	[ $m = gsi_mppc_rob ] && n="$n gsi_pex"
	[ $m = gsi_siderem ] && n="$n gsi_sam"
	[ $m = gsi_tacquila ] && n="$n gsi_sam"
	[ $m = pnpi_cros3 ] && n="$n gsi_sam"
	[ $m = gsi_tamex ] && n="$n gsi_pex"
	[ $m = gsi_pexaria ] && n="$n gsi_etherbone"
	[ $m = gsi_tridi ] && n="$n trloii"
	[ $m = gsi_vetar ] && n="$n gsi_etherbone"
	[ $m = gsi_vulom ] && n="$n trloii"
	[ $m = gsi_rfx1 ] && n="$n trloii"
	[ $m = mesytec_madc32 ] && n="$n mesytec_mxdc32"
	[ $m = mesytec_mqdc32 ] && n="$n mesytec_mxdc32"
	[ $m = mesytec_mtdc32 ] && n="$n mesytec_mxdc32"
	[ $m = mesytec_mdpp16scp ] && n="$n mesytec_mdpp mesytec_mxdc32"
	[ $m = mesytec_mdpp16qdc ] && n="$n mesytec_mdpp mesytec_mxdc32"
	[ $m = mesytec_mdpp32scp ] && n="$n mesytec_mdpp mesytec_mxdc32"
	[ $m = mesytec_mdpp32qdc ] && n="$n mesytec_mdpp mesytec_mxdc32"
	[ $m = mesytec_vmmr8 ] && n="$n mesytec_mxdc32"
	echo $m = $n
	make -C .. $jobs BUILD_DIR_PREFIX=mod_${m}_ MODULE_LIST="dummy $n" || exit 1
done
