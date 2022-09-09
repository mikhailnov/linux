// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * clk-baikal.c - Baikal-M clock driver.
 *
 * Copyright (C) 2015,2016,2020,2021 Baikal Electronics JSC
 * Authors:
 *   Ekaterina Skachko <ekaterina.skachko@baikalelectronics.ru>
 *   Alexey Sheplyakov <asheplyakov@basealt.ru>
 */

#include <linux/arm-smccc.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#define CMU_PLL_SET_RATE		0
#define CMU_PLL_GET_RATE		1
#define CMU_PLL_ENABLE			2
#define CMU_PLL_DISABLE			3
#define CMU_PLL_ROUND_RATE		4
#define CMU_PLL_IS_ENABLED		5
#define CMU_CLK_CH_SET_RATE		6
#define CMU_CLK_CH_GET_RATE		7
#define CMU_CLK_CH_ENABLE		8
#define CMU_CLK_CH_DISABLE		9
#define CMU_CLK_CH_ROUND_RATE		10
#define CMU_CLK_CH_IS_ENABLED		11

struct baikal_clk_cmu {
	struct clk_hw	hw;
	uint32_t	cmu_id;
	unsigned int	parent;
	const char	*name;
	uint32_t	is_clk_ch;
};

#define to_baikal_cmu(_hw) container_of(_hw, struct baikal_clk_cmu, hw)

/* Pointer to the place on handling SMC CMU calls in monitor */
#define BAIKAL_SMC_LCRU_ID	0x82000000

static int baikal_clk_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct baikal_clk_cmu *pclk = to_baikal_cmu(hw);
	uint32_t cmd;

	if (pclk->is_clk_ch) {
		cmd = CMU_CLK_CH_ENABLE;
	} else {
		cmd = CMU_PLL_ENABLE;
	}

	arm_smccc_smc(BAIKAL_SMC_LCRU_ID, pclk->cmu_id, cmd, 0,
			pclk->parent, 0, 0, 0, &res);

	pr_debug("%s(%s, %s@0x%x): %s\n",
			__func__,
			pclk->name,
			pclk->is_clk_ch ? "clkch" : "pll",
			pclk->cmu_id,
			res.a0 ? "error" : "ok");

	return res.a0;
}

static void baikal_clk_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct baikal_clk_cmu *pclk = to_baikal_cmu(hw);
	uint32_t cmd;

	if (pclk->is_clk_ch) {
		cmd = CMU_CLK_CH_DISABLE;
	} else {
		cmd = CMU_PLL_DISABLE;
	}

	arm_smccc_smc(BAIKAL_SMC_LCRU_ID, pclk->cmu_id, cmd, 0,
			pclk->parent, 0, 0, 0, &res);

	pr_debug("%s(%s, %s@0x%x): %s\n",
			__func__,
			pclk->name,
			pclk->is_clk_ch ? "clkch" : "pll",
			pclk->cmu_id,
			res.a0 ? "error" : "ok");
}

static int baikal_clk_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	struct baikal_clk_cmu *pclk = to_baikal_cmu(hw);
	uint32_t cmd;

	if (pclk->is_clk_ch) {
		cmd = CMU_CLK_CH_IS_ENABLED;
	} else {
		cmd = CMU_PLL_IS_ENABLED;
	}

	arm_smccc_smc(BAIKAL_SMC_LCRU_ID, pclk->cmu_id, cmd, 0,
			pclk->parent, 0, 0, 0, &res);

	pr_debug("%s(%s, %s@0x%x): %s\n",
			__func__,
			pclk->name,
			pclk->is_clk_ch ? "clkch" : "pll",
			pclk->cmu_id,
			res.a0 ? "true" : "false");

	return res.a0;
}

static unsigned long baikal_clk_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	struct arm_smccc_res res;
	struct baikal_clk_cmu *pclk = to_baikal_cmu(hw);
	uint32_t cmd;
	unsigned long parent;

	if (pclk->is_clk_ch) {
		cmd = CMU_CLK_CH_GET_RATE;
		parent = pclk->parent;
	} else {
		cmd = CMU_PLL_GET_RATE;
		parent= parent_rate;
	}

	arm_smccc_smc(BAIKAL_SMC_LCRU_ID, pclk->cmu_id, cmd, 0,
			parent, 0, 0, 0, &res);

	pr_debug("%s(%s, %s@0x%x): %ld Hz\n",
			__func__,
			pclk->name,
			pclk->is_clk_ch ? "clkch" : "pll",
			pclk->cmu_id,
			res.a0);

	return res.a0;
}

static int baikal_clk_set_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long parent_rate)
{
	struct arm_smccc_res res;
	struct baikal_clk_cmu *pclk = to_baikal_cmu(hw);
	uint32_t cmd;
	unsigned long parent;

	if (pclk->is_clk_ch) {
		cmd = CMU_CLK_CH_SET_RATE;
		parent = pclk->parent;
	} else {
		cmd = CMU_PLL_SET_RATE;
		parent = parent_rate;
	}

	arm_smccc_smc(BAIKAL_SMC_LCRU_ID, pclk->cmu_id, cmd, rate,
			parent, 0, 0, 0, &res);

	pr_debug("%s(%s, %s@0x%x, %ld Hz): %s\n",
			__func__,
			pclk->name,
			pclk->is_clk_ch ? "clkch" : "pll",
			pclk->cmu_id,
			rate,
			res.a0 ? "error" : "ok");

	return res.a0;
}

static long baikal_clk_round_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long *prate)
{
	struct arm_smccc_res res;
	struct baikal_clk_cmu *pclk = to_baikal_cmu(hw);
	unsigned long parent;
	uint32_t cmd;

	if (pclk->is_clk_ch) {
		cmd = CMU_CLK_CH_ROUND_RATE;
		parent = pclk->parent;
	} else {
		cmd = CMU_PLL_ROUND_RATE;
		parent = *prate;
	}

	arm_smccc_smc(BAIKAL_SMC_LCRU_ID, pclk->cmu_id, cmd, rate,
			parent, 0, 0, 0, &res);

	pr_debug("%s(%s, %s@0x%x): %ld Hz\n",
			__func__,
			pclk->name,
			pclk->is_clk_ch ? "clkch" : "pll",
			pclk->cmu_id,
			res.a0);

	return res.a0;
}

static const struct clk_ops be_clk_pll_ops = {
	.enable = baikal_clk_enable,
	.disable = baikal_clk_disable,
	.is_enabled = baikal_clk_is_enabled,
	.recalc_rate = baikal_clk_recalc_rate,
	.set_rate = baikal_clk_set_rate,
	.round_rate = baikal_clk_round_rate
};

static int __init baikal_clk_probe(struct device_node *node)
{
	struct clk_init_data init;
	struct clk_init_data *init_ch;
	struct baikal_clk_cmu *cmu;
	struct baikal_clk_cmu **cmu_ch;

	struct clk *clk;
	struct clk_onecell_data *clk_ch;

	int number, i = 0;
	u32 rc, index;
	struct property *prop;
	const __be32 *p;
	const char *clk_ch_name;
	const char *parent_name;

	cmu = kzalloc(sizeof(struct baikal_clk_cmu), GFP_KERNEL);
	if (!cmu) {
		pr_err("%s: could not allocate CMU clk\n", __func__);
		return -ENOMEM;
	}

	of_property_read_string(node, "clock-output-names", &cmu->name);
	of_property_read_u32(node, "clock-frequency", &cmu->parent);
	of_property_read_u32(node, "cmu-id", &cmu->cmu_id);

	parent_name = of_clk_get_parent_name(node, 0);

	/* Setup clock init structure */
	init.parent_names = &parent_name;
	init.num_parents = 1;
	init.name = cmu->name;
	init.ops = &be_clk_pll_ops;
	init.flags = CLK_IGNORE_UNUSED;

	cmu->hw.init = &init;
	cmu->is_clk_ch = 0;

	/* Register the clock */
	pr_debug("%s: add %s, parent %s\n", __func__, cmu->name, parent_name ? parent_name : "null");
	clk = clk_register(NULL, &cmu->hw);

	if (IS_ERR(clk)) {
		pr_err("%s: could not register clk %s\n", __func__, cmu->name);
		return -ENOMEM;
	}

	/* Register the clock for lookup */
	rc = clk_register_clkdev(clk, cmu->name, NULL);
	if (rc != 0) {
		pr_err("%s: could not register lookup clk %s\n",
			__func__, cmu->name);
	}

	clk_prepare_enable(clk);

	number = of_property_count_u32_elems(node, "clock-indices");

	if (number > 0) {
		clk_ch = kmalloc(sizeof(struct clk_onecell_data), GFP_KERNEL);
 		if (!clk_ch) {
			pr_err("%s: could not allocate CMU clk channel\n", __func__);
			return -ENOMEM;
 		}

		/* Get the last index to find out max number of children*/
		of_property_for_each_u32(node, "clock-indices", prop, p, index) {
			;
		}

		clk_ch->clks = kcalloc(index + 1, sizeof(struct clk *), GFP_KERNEL);
		clk_ch->clk_num = index + 1;
		cmu_ch = kcalloc((index + 1), sizeof(struct baikal_clk_cmu *), GFP_KERNEL);
		if (!cmu_ch) {
			kfree(clk_ch);
			return -ENOMEM;
		}
		init_ch = kcalloc((number + 1), sizeof(struct clk_init_data), GFP_KERNEL);
		if (!init_ch) {
			pr_err("%s: could not allocate CMU init structure \n", __func__);
			kfree(cmu_ch);
			kfree(clk_ch);
			return -ENOMEM;
		}

		of_property_for_each_u32(node, "clock-indices", prop, p, index) {
			of_property_read_string_index(node, "clock-names",
							i, &clk_ch_name);
			pr_info("%s: clkch <%s>, index %d, i %d\n", __func__, clk_ch_name, index, i);
			init_ch[i].parent_names = &cmu->name;
			init_ch[i].num_parents = 1;
			init_ch[i].name = clk_ch_name;
			init_ch[i].ops = &be_clk_pll_ops;
			init_ch[i].flags = CLK_IGNORE_UNUSED;

			cmu_ch[index] = kzalloc(sizeof(struct baikal_clk_cmu), GFP_KERNEL);
			if (!cmu_ch[index]) {
				pr_err("%s: could not allocate baikal_clk_cmu structure\n", __func__);
				return -ENOMEM;
			}
			cmu_ch[index]->name = clk_ch_name;
			cmu_ch[index]->cmu_id = index;
			cmu_ch[index]->parent = cmu->cmu_id;
			cmu_ch[index]->is_clk_ch = 1;
			cmu_ch[index]->hw.init = &init_ch[i];
			clk_ch->clks[index] = clk_register(NULL, &cmu_ch[index]->hw);

			if (IS_ERR(clk_ch->clks[index])) {
				pr_err("%s: could not register clk %s\n", __func__, clk_ch_name);
			}

			rc = clk_register_clkdev(clk_ch->clks[index], clk_ch_name, NULL);
			if (rc != 0) {
				pr_err("%s: could not register lookup clk %s\n",
					__func__, clk_ch_name);
			}

			clk_prepare_enable(clk_ch->clks[index]);
			i++;
		}

		return of_clk_add_provider(node, of_clk_src_onecell_get, clk_ch);
	}

	return of_clk_add_provider(node, of_clk_src_simple_get, clk);
}

static void __init baikal_clk_init(struct device_node *np)
{
	int err;
	err = baikal_clk_probe(np);
	if (err) {
		panic("%s: failed to probe clock %pOF: %d\n", __func__, np, err);
	} else {
		pr_info("%s: successfully probed %pOF\n", __func__, np);
	}
}
CLK_OF_DECLARE_DRIVER(baikal_cmu, "baikal,cmu", baikal_clk_init);
