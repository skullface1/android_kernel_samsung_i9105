/************************************************************************************************/
/*                                                                                              */
/*  Copyright 2012  Broadcom Corporation                                                        */
/*                                                                                              */
/*     Unless you and Broadcom execute a separate written software license agreement governing  */
/*     use of this software, this software is licensed to you under the terms of the GNU        */
/*     General Public License version 2 (the GPL), available at                                 */
/*                                                                                              */
/*          http://www.broadcom.com/licenses/GPLv2.php                                          */
/*                                                                                              */
/*     with the following added to such license:                                                */
/*                                                                                              */
/*     As a special exception, the copyright holders of this software give you permission to    */
/*     link this software with independent modules, and to copy and distribute the resulting    */
/*     executable under terms of your choice, provided that you also meet, for each linked      */
/*     independent module, the terms and conditions of the license of that module.              */
/*     An independent module is a module which is not derived from this software.  The special  */
/*     exception does not apply to any modifications of the software.                           */
/*                                                                                              */
/*     Notwithstanding the above, under no circumstances may you combine this software in any   */
/*     way with any other Broadcom software provided under a license other than the GPL,        */
/*     without Broadcom's express prior written consent.                                        */
/*                                                                                              */
/************************************************************************************************/

#ifndef __MACH_CAPRI_CPUTIME_H
#define __MACH_CAPRI_CPUTIME_H

#include <linux/atomic.h>
extern atomic_t nohz_pause;
static inline void pause_nohz(void)
{
        atomic_inc(&nohz_pause);
}
static inline void resume_nohz(void)
{
        atomic_dec(&nohz_pause);
}
static inline int kona_nohz_delay(int cpu)
{
        return atomic_read(&nohz_pause);
}

#define arch_needs_cpu(cpu) kona_nohz_delay(cpu)

#endif /*__MACH_CAPRI_CPUTIME_H */
