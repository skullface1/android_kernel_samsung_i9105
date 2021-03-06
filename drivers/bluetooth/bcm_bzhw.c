/*******************************************************************************
* Copyright 2010 Broadcom Corporation.  All rights reserved.
*
* @file	drivers/bluetooth/brcm_bzhw.c
*
* Unless you and Broadcom execute a separate written software license agreement
* governing use of this software, this software is licensed to you under the
* terms of the GNU General Public License version 2, available at
* http://www.gnu.org/copyleft/gpl.html (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a license
* other than the GPL, without Broadcom's express prior written consent.
*******************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <linux/broadcom/bcm_bzhw.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/interrupt.h>
#include <asm/irq.h>
#include <linux/uaccess.h>
#include <linux/tty.h>
#include <linux/delay.h>

#ifdef CONFIG_BCM_BZHW
#define BZHW_CLOCK_ENABLE 1
#define BZHW_CLOCK_DISABLE 0

#ifndef BZHW_BT_WAKE_ASSERT
#define BZHW_BT_WAKE_ASSERT 1
#endif
#ifndef BZHW_BT_WAKE_DEASSERT
#define BZHW_BT_WAKE_DEASSERT (!(BZHW_BT_WAKE_ASSERT))
#endif

#ifndef BZHW_HOST_WAKE_ASSERT
#define BZHW_HOST_WAKE_ASSERT 1
#endif
#ifndef BZHW_HOST_WAKE_DEASSERT
#define BZHW_HOST_WAKE_DEASSERT (!(BZHW_HOST_WAKE_ASSERT))
#endif


/* BZHW states */
enum bzhw_states_e {
	BZHW_ASLEEP,
	BZHW_ASLEEP_TO_AWAKE,
	BZHW_AWAKE,
	BZHW_AWAKE_TO_ASLEEP
};

struct bcmbzhw_struct *priv_g;

static int bcm_bzhw_init_clock(struct bcmbzhw_struct *priv)
{
	int ret;
	if (priv == NULL) {
		pr_err
		("%s BLUETOOTH: core structure not initialized\n", __func__);
		return -1;
	}
	ret =
	    pi_mgr_qos_add_request(&priv->qos_node_bw, "bt_qos_node_bw",
				   PI_MGR_PI_ID_ARM_SUB_SYSTEM,
				   PI_MGR_QOS_DEFAULT_VALUE);
	if (ret < 0) {
		pr_err
	("%s BLUETOOTH:bt_qosNode_bw addition failed\n", __func__);
		return -1;
	}

	ret =
	    pi_mgr_qos_add_request(&priv->qos_node_hw, "bt_qos_node_hw",
				   PI_MGR_PI_ID_ARM_SUB_SYSTEM,
				   PI_MGR_QOS_DEFAULT_VALUE);
	if (ret < 0) {
		pr_err
	("%s BLUETOOTH:bt_qosNode_hw addition failed\n", __func__);
		return -1;
	}

	pr_debug("%s BLUETOOTH:bt_qosNode SUCCESFULL\n", __func__);
	return 0;
}


void bcm_bzhw_request_clock_on(struct pi_mgr_qos_node *node)
{
	int ret;
	pr_debug("%s: bcm_bzhw_request_clock_on\n", __func__);
	if (node == NULL) {
		pr_err("%s: Node passed is null\n", __func__);
		return;
	}
	ret = pi_mgr_qos_request_update(node, 0);
	if (ret == 0)
		pr_debug
	("%s BLUETOOTH:UART OUT of retention\n", __func__);
	else
		pr_err
	("%s BLUETOOTH: UART OUT OF retention FAILED\n", __func__);

}
void bcm_bzhw_request_clock_off(struct pi_mgr_qos_node *node)
{
	int ret;
	if (node == NULL) {
		pr_err("%s: PI node passed is null\n", __func__);
		return;
	}
	pr_debug("%s: bcm_bzhw_request_clock_off\n", __func__);
	ret = pi_mgr_qos_request_update(node, PI_MGR_QOS_DEFAULT_VALUE);
	if (ret == 0)
		pr_debug
	("%s BLUETOOTH:UART in retention\n", __func__);
	else
		pr_err
	("%s BLUETOOTH:UART NOT in retention\n", __func__);

}

int bcm_bzhw_assert_bt_wake(int bt_wake_gpio, struct pi_mgr_qos_node *lqos_node,
				struct tty_struct *tty)
{
	struct uart_state *state;
	struct uart_port *port;
	int rc;
	if (lqos_node == NULL || tty == NULL) {
		pr_err("%s: Null pointers passed is null\n", __func__);
		return -1;
	}
	state = tty->driver_data;
	port = state->uart_port;
#ifdef CONFIG_HAS_WAKELOCK
	if (priv_g != NULL) {
		if (!wake_lock_active(&priv_g->bzhw_data.bt_wake_lock))
			wake_lock(&priv_g->bzhw_data.bt_wake_lock);
	}
#endif
	rc = pi_mgr_qos_request_update(lqos_node, 0);
	 gpio_set_value(bt_wake_gpio, BZHW_BT_WAKE_ASSERT);
	pr_debug("%s BLUETOOTH:ASSERT BT_WAKE\n", __func__);
	return 0;
}

int bcm_bzhw_deassert_bt_wake(int bt_wake_gpio, int host_wake_gpio)
{
	int host_wake=-1;
	if (bt_wake_gpio == -1) {
		pr_err("%s: bt_wake_gpio=%d\n", __func__,
		       bt_wake_gpio);
		return -EFAULT;
	}

	gpio_set_value(bt_wake_gpio, BZHW_BT_WAKE_DEASSERT);
	
	host_wake = gpio_get_value(priv_g->bzhw_data.gpio_host_wake);
	if(host_wake == BZHW_HOST_WAKE_DEASSERT) {
	   pr_debug("BLUETOOTH:DEASSERT BT_WAKE; no activity on host wake also so cleanup\n");
	pi_mgr_qos_request_update(&priv_g->qos_node_hw, PI_MGR_QOS_DEFAULT_VALUE);
#ifdef CONFIG_HAS_WAKELOCK
	if (priv_g != NULL) {
		if (wake_lock_active(&priv_g->bzhw_data.host_wake_lock))
			wake_unlock(&priv_g->bzhw_data.host_wake_lock);
	}
#endif
	}

	pr_debug("BLUETOOTH:DEASSERT BT_WAKE\n");
	pi_mgr_qos_request_update(&priv_g->qos_node_bw, PI_MGR_QOS_DEFAULT_VALUE);
#ifdef CONFIG_HAS_WAKELOCK
	if (priv_g != NULL) {
		if (wake_lock_active(&priv_g->bzhw_data.bt_wake_lock))
			wake_unlock(&priv_g->bzhw_data.bt_wake_lock);
	}
#endif

	return 0;

}

static int bcm_bzhw_init_bt_wake(struct bcmbzhw_struct *priv)
{
	int rc;

	rc = gpio_request(priv->pdata->gpio_bt_wake, "BT Power Mgmt");
	if (rc) {
		pr_err
	("%s: failed to configure BT Power Mgmt: gpio_request err%d\n",
		     __func__, rc);
		return rc;
	}

	rc = gpio_direction_output(priv->pdata->gpio_bt_wake,
				BZHW_BT_WAKE_DEASSERT);
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&priv->bzhw_data.bt_wake_lock,
			WAKE_LOCK_SUSPEND, "BTWAKE");
#endif
	priv->bzhw_data.gpio_bt_wake = priv->pdata->gpio_bt_wake;
	return 0;

}

static void bcm_bzhw_clean_bt_wake(struct bcmbzhw_struct *priv)
{
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&priv->bzhw_data.bt_wake_lock);
#endif
	gpio_free((unsigned)priv->pdata->gpio_bt_wake);

}

static irqreturn_t bcm_bzhw_host_wake_isr(int irq, void *dev)
{
	unsigned int host_wake;
	unsigned int bt_wake;
	unsigned long flags;
	int ret = -1;
	struct bcmbzhw_struct *priv = (struct bcmbzhw_struct *)dev;
	if (priv == NULL) {
		pr_err("%s BLUETOOTH: Error data pointer is null\n", __func__);
		return IRQ_HANDLED;
	}
	spin_lock_irqsave(&priv->bzhw_lock, flags);
	host_wake = gpio_get_value(priv->bzhw_data.gpio_host_wake);
	if (BZHW_HOST_WAKE_ASSERT == host_wake) {
		pr_debug("%s BLUETOOTH: hostwake ISR Assert\n", __func__);
		/* wake up peripheral clock */
#ifdef CONFIG_HAS_WAKELOCK
	if (!wake_lock_active(&priv->bzhw_data.host_wake_lock))
		wake_lock(&priv->bzhw_data.host_wake_lock);
#endif
		ret = pi_mgr_qos_request_update(&priv->qos_node_hw, 0);
		priv->bzhw_state = BZHW_AWAKE;
		spin_unlock_irqrestore(&priv->bzhw_lock, flags);
	} else {
		pr_debug("%s BLUETOOTH: hostwake ISR DeAssert\n", __func__);
		host_wake = gpio_get_value(priv->bzhw_data.gpio_host_wake);
		bt_wake = gpio_get_value(priv->bzhw_data.gpio_bt_wake);
		if(bt_wake == BZHW_BT_WAKE_ASSERT){
			pr_debug("%s BLUETOOTH: hostwake ISR DeAssert but BT_Wake is asserted high so let BT Wake handle this \n", __func__);
			spin_unlock_irqrestore(&priv->bzhw_lock, flags);
		} else {
			if (priv->bzhw_state == BZHW_ASLEEP) {
			       pr_debug("%s BLUETOOTH: hostwake already sleeping \n", __func__);
   				#ifdef CONFIG_HAS_WAKELOCK
				if (wake_lock_active(&priv->bzhw_data.host_wake_lock))
					wake_unlock(&priv->bzhw_data.host_wake_lock);
				#endif
				   spin_unlock_irqrestore(&priv->bzhw_lock, flags);
			} else {
			       pr_debug("%s BLUETOOTH: hostwake idr ready to sleep \n", __func__);
				pi_mgr_qos_request_update(&priv->qos_node_hw,
				      PI_MGR_QOS_DEFAULT_VALUE);
				priv->bzhw_state = BZHW_ASLEEP;
				#ifdef CONFIG_HAS_WAKELOCK
				if (wake_lock_active(&priv->bzhw_data.host_wake_lock))
					wake_unlock(&priv->bzhw_data.host_wake_lock);
				#endif
				spin_unlock_irqrestore(&priv->bzhw_lock, flags);
			}
		}
	}
	return IRQ_HANDLED;
}

static int bcm_bzhw_init_hostwake(struct bcmbzhw_struct *priv)
{
	int rc;

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&priv->bzhw_data.host_wake_lock, WAKE_LOCK_SUSPEND,
		       "HOSTWAKE");
#endif

	rc = gpio_request(priv->pdata->gpio_host_wake, "BT Host Power Mgmt");
	if (rc) {
		pr_err
	    ("%s: failed to configure BT Host Mgmt: gpio_request err=%d\n",
		     __func__, rc);
		return rc;
	}
	rc = gpio_direction_input(priv->pdata->gpio_host_wake);
	rc = bcm_bzhw_init_clock(priv);
	priv->bzhw_data.gpio_host_wake = priv->pdata->gpio_host_wake;

	return 0;
}

static void bcm_bzhw_clean_host_wake(struct bcmbzhw_struct *priv)
{
	if (priv->pdata->gpio_host_wake == -1)
		return;

	free_irq(priv->bzhw_data.host_irq, bcm_bzhw_host_wake_isr);

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&priv->bzhw_data.host_wake_lock);
#endif
	gpio_free((unsigned)priv->pdata->gpio_host_wake);

}

struct bcmbzhw_struct *bcm_bzhw_start(struct tty_struct* tty)
{
	struct uart_state *state;
	struct uart_port *port;
	int rc;
	if (priv_g != NULL) {
		pr_debug("%s: BLUETOOTH: data pointer is valid\n", __func__);
		priv_g->bcmtty = tty;
		state = tty->driver_data;
		port = state->uart_port;
		priv_g->uport = port;
		rc = gpio_to_irq(priv_g->bzhw_data.gpio_host_wake);
		if (rc < 0) {
			pr_err
		   ("%s: failed to configure BT Host Mgmt err=%d\n",
		     __func__, rc);
		   goto exit_lock_host_wake;
		}
		priv_g->bzhw_data.host_irq = rc;
		priv_g->bzhw_state = BZHW_AWAKE;

		pr_debug("%s: BLUETOOTH: request_irq host_irq=%d\n",
			__func__, priv_g->bzhw_data.host_irq);
		rc = request_irq(
			priv_g->bzhw_data.host_irq, bcm_bzhw_host_wake_isr,
			 (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
			  IRQF_NO_SUSPEND), "bt_host_wake", priv_g);
		 if (rc) {
			pr_err
		("%s: failed to configure BT Host Mgmt:request_irq err=%d\n",
			     __func__, rc);
		}
		return priv_g;
	} else {
		pr_err("%s: BLUETOOTH:data pointer null, NOT initialized\n",
			__func__);
		return NULL;
	}
exit_lock_host_wake:
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&priv_g->bzhw_data.host_wake_lock);
#endif
	gpio_free((unsigned)priv_g->pdata->gpio_host_wake);
	return NULL;

}


void bcm_bzhw_stop(struct bcmbzhw_struct *hw_val)
{
	if (hw_val == NULL)
		return;
	pr_debug("%s: BLUETOOTH: hw_val->bzhw_data.host_irq=%d\n",
		__func__, hw_val->bzhw_data.host_irq);
	hw_val->bzhw_state = BZHW_ASLEEP;

	pi_mgr_qos_request_update(&hw_val->qos_node_bw,
				    PI_MGR_QOS_DEFAULT_VALUE);
	pi_mgr_qos_request_update(&hw_val->qos_node_hw,
				    PI_MGR_QOS_DEFAULT_VALUE);

	if (wake_lock_active(&hw_val->bzhw_data.host_wake_lock))
		wake_unlock(&hw_val->bzhw_data.host_wake_lock);

	if (wake_lock_active(&hw_val->bzhw_data.bt_wake_lock))
		wake_unlock(&hw_val->bzhw_data.bt_wake_lock);

	free_irq(hw_val->bzhw_data.host_irq, hw_val);
}

static int bcm_bzhw_probe(struct platform_device *pdev)
{

	int rc = 0;
	priv_g = kzalloc(sizeof(*priv_g), GFP_ATOMIC);
	if (!priv_g)
		return -ENOMEM;
	spin_lock_init(&priv_g->bzhw_lock);
	priv_g->pdata =
		(struct bcm_bzhw_platform_data *)pdev->dev.platform_data;
	priv_g->bzhw_data.gpio_bt_wake = -1;
	priv_g->bzhw_data.gpio_host_wake = -1;
	priv_g->bzhw_data.host_irq = -1;
	if (!priv_g->pdata) {
		pr_err("%s: platform data is not set\n", __func__);
		return -ENODEV;
	}
	pr_info("%s: gpio_bt_wake=%d, gpio_host_wake=%d\n", __func__,
		priv_g->pdata->gpio_bt_wake, priv_g->pdata->gpio_host_wake);

	priv_g->uport = NULL;
	priv_g->bzhw_state = BZHW_ASLEEP;
	rc = bcm_bzhw_init_bt_wake(priv_g);
	rc = bcm_bzhw_init_hostwake(priv_g);

	if (rc)
		return rc;

	return 0;
}

static int bcm_bzhw_remove(struct platform_device *pdev)
{
	struct bcm_bzhw_platform_data *pdata =
		(struct bcm_bzhw_platform_data *)pdev->dev.platform_data;

	if (pdata) {
		bcm_bzhw_clean_bt_wake(priv_g);
		bcm_bzhw_clean_host_wake(priv_g);
		if (pi_mgr_qos_request_remove(&priv_g->qos_node_bw))
			pr_info("%s failed to unregister qos_bw client\n",
				__func__);

		if (pi_mgr_qos_request_remove(&priv_g->qos_node_hw))
			pr_info("%s failed to unregister qos_hw client\n",
				__func__);

	}
	kfree(priv_g);
	return 0;
}

static struct platform_driver bcm_bzhw_platform_driver = {
	.probe = bcm_bzhw_probe,
	.remove = bcm_bzhw_remove,
	.driver = {
		   .name = "bcm_bzhw",
		   .owner = THIS_MODULE,
		   },
};

static int __init bcm_bzhw_init(void)
{
	int rc = platform_driver_register(&bcm_bzhw_platform_driver);
	if (rc)
		pr_err("%s bcm_bzhw_platform_driver_register failed err=%d\n",
			__func__, rc);
	else
		pr_info("%s bcm_bzhw_init success\n", __func__);

	return rc;
}

static void __exit bcm_bzhw_exit(void)
{
	platform_driver_unregister(&bcm_bzhw_platform_driver);
}

module_init(bcm_bzhw_init);
module_exit(bcm_bzhw_exit);

MODULE_DESCRIPTION("bcm_bzhw");
MODULE_AUTHOR("Broadcom");
MODULE_LICENSE("GPL");
#endif
