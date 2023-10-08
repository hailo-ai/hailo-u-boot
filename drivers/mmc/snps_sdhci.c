// SPDX-License-Identifier: GPL-2.0+
#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <malloc.h>
#include <reset-uclass.h>
#include <sdhci.h>
#include <dm/of_access.h>
#include <clk.h>
#include <linux/delay.h>
#include <linux/bitfield.h>

#define DWCMSHC_EMMC_CTRL_R  			0x0000052C
#define DWCMSHC_EMMC_CTRL_R__CARD_IS_EMMC            BIT(0)
#define DWCMSHC_EMMC_CTRL_R__DISABLE_DATA_CRC_CHK    BIT(1)
#define DWCMSHC_EMMC_CTRL_R__EMMC_RST_N		     BIT(2)
#define DWCMSHC_EMMC_CTRL_R__EMMC_RST_N_OE	     BIT(3)

#define DWCMSHC_CMDPAD_CNFG  			0x00000304 
#define DWCMSHC_CMDPAD_CNFG__RXSEL GENMASK(2, 0)
#define DWCMSHC_CMDPAD_CNFG__WEAKPULL_EN GENMASK(4, 3)
#define DWCMSHC_CMDPAD_CNFG__TXSLEW_CTRL_P GENMASK(8,5)
#define DWCMSHC_CMDPAD_CNFG__TXSLEW_CTRL_N GENMASK(12,9)

#define DWCMSHC_DATPAD_CNFG 			0x00000306 
#define DWCMSHC_DATPAD_CNFG__RXSEL GENMASK(2, 0)
#define DWCMSHC_DATPAD_CNFG__WEAKPULL_EN GENMASK (4, 3)
#define DWCMSHC_DATPAD_CNFG__TXSLEW_CTRL_P GENMASK(8,5)
#define DWCMSHC_DATPAD_CNFG__TXSLEW_CTRL_N GENMASK(12,9)

#define DWCMSHC_RSTNPAD_CNFG			0x0000030C 
#define DWCMSHC_RSTNPAD_CNFG__RXSEL GENMASK(2, 0)
#define DWCMSHC_RSTNPAD_CNFG__WEAKPULL_EN GENMASK (4, 3)
#define DWCMSHC_RSTNPAD_CNFG__TXSLEW_CTRL_P GENMASK(8,5)
#define DWCMSHC_RSTNPAD_CNFG__TXSLEW_CTRL_N GENMASK(12,9)

#define DWCMSHC_CLKPAD_CNFG			0x00000308 
#define DWCMSHC_CLKPAD_CNFG__RXSEL GENMASK(2, 0)
#define DWCMSHC_CLKPAD_CNFG__WEAKPULL_EN GENMASK(4, 3)
#define DWCMSHC_CLKPAD_CNFG__TXSLEW_CTRL_P GENMASK(8,5)
#define DWCMSHC_CLKPAD_CNFG__TXSLEW_CTRL_N GENMASK(12,9)

#define DWCMSHC_PHY_CNFG			0x00000300
#define DWCMSHC_PHY_CNFG__PHY_RSTN BIT(0)
#define DWCMSHC_PHY_CNFG__PHY_PWRGOOD BIT(1)
#define DWCMSHC_PHY_CNFG__PAD_SP GENMASK(19,16)
#define DWCMSHC_PHY_CNFG__PAD_SN GENMASK(23,20)

#define DWCMSHC_CLK_CTRL_R			0x0000002C
#define DWCMSHC_CLK_CTRL_R__PLL_ENABLE	BIT(3)

#define DWCMSHC_SDCLKDL_CNFG			0x0000031D
#define DWCMSHC_SDCLKDL_CNFG__EXTDLY_EN BIT(0)

#define DWCMSHC_SDCLKDL_DC			0x0000031E
#define DWCMSHC_SDCLKDL_DC__CCKDL_DC GENMASK(6,0)

enum pad_config {
	TXSLEW_CTRL_N = 0,
	TXSLEW_CTRL_P = 1,
	WEAKPULL_EN = 2,
	RXSEL = 3,
	PAD_CONFIG_MAX = 4
};
enum clk_delay_config {
	EXTDLY_EN = 0,
        CCKDL_DC = 1,
	CLK_DELAY_CONFIG_MAX = 2
};
enum drive_strength_config {
	PAD_SP = 0,
	PAD_SN = 1,
	DS_CONFIG_MAX = 2
};

typedef struct hailo15_phy_config {
	uint32_t card_is_emmc;
	uint32_t cmd_pad[PAD_CONFIG_MAX];
	uint32_t dat_pad[PAD_CONFIG_MAX];
	uint32_t rst_pad[PAD_CONFIG_MAX];
	uint32_t clk_pad[PAD_CONFIG_MAX];
	uint32_t clk_delay[CLK_DELAY_CONFIG_MAX];
	uint32_t drive_strength[DS_CONFIG_MAX];
} hailo15_phy_config;
struct snps_sdhci_plat {
	struct mmc_config cfg;
	struct mmc mmc;
	struct reset_ctl reset;
	struct clk core_clk;
	struct clk bus_clk;
	struct clk card_clk;
	struct clk div_clk_bypass;
	bool is_clk_divider_bypass;
	hailo15_phy_config sdio_phy_config;
};

static void sdhci_hailo15_phy_config(struct sdhci_host *host, struct udevice *dev, hailo15_phy_config* sdio_phy_config)
{
	uint32_t reg32 = 0;
	uint16_t reg16 = 0;
	uint8_t  reg8 = 0;      

	reg32 = sdhci_readl(host, DWCMSHC_EMMC_CTRL_R);
	reg32 &= ~DWCMSHC_EMMC_CTRL_R__CARD_IS_EMMC;
	if (sdio_phy_config->card_is_emmc) {
		reg32 |= DWCMSHC_EMMC_CTRL_R__CARD_IS_EMMC;
	}
	sdhci_writel(host, reg32, DWCMSHC_EMMC_CTRL_R);
	
        /* CMD PAD configuration */
        reg16 = sdhci_readw(host, DWCMSHC_CMDPAD_CNFG);
	reg16 &= ~DWCMSHC_CMDPAD_CNFG__TXSLEW_CTRL_N;
	reg16 |= FIELD_PREP(DWCMSHC_CMDPAD_CNFG__TXSLEW_CTRL_N, sdio_phy_config->cmd_pad[TXSLEW_CTRL_N]);
	reg16 &= ~DWCMSHC_CMDPAD_CNFG__TXSLEW_CTRL_P;
	reg16 |= FIELD_PREP(DWCMSHC_CMDPAD_CNFG__TXSLEW_CTRL_P, sdio_phy_config->cmd_pad[TXSLEW_CTRL_P]);
	reg16 &= ~DWCMSHC_CMDPAD_CNFG__WEAKPULL_EN;
	reg16 |= FIELD_PREP(DWCMSHC_CMDPAD_CNFG__WEAKPULL_EN, sdio_phy_config->cmd_pad[WEAKPULL_EN]);
	reg16 &= ~DWCMSHC_CMDPAD_CNFG__RXSEL;
	reg16 |= FIELD_PREP(DWCMSHC_CMDPAD_CNFG__RXSEL, sdio_phy_config->cmd_pad[RXSEL]);
	sdhci_writew(host, reg16, DWCMSHC_CMDPAD_CNFG);

        /* DAT PAD configuration */
        reg16 = sdhci_readw(host, DWCMSHC_DATPAD_CNFG);
	reg16 &= ~DWCMSHC_DATPAD_CNFG__TXSLEW_CTRL_N;
	reg16 |= FIELD_PREP(DWCMSHC_DATPAD_CNFG__TXSLEW_CTRL_N, sdio_phy_config->dat_pad[TXSLEW_CTRL_N]);
	reg16 &= ~DWCMSHC_DATPAD_CNFG__TXSLEW_CTRL_P;
	reg16 |= FIELD_PREP(DWCMSHC_DATPAD_CNFG__TXSLEW_CTRL_P, sdio_phy_config->dat_pad[TXSLEW_CTRL_P]);
	reg16 &= ~DWCMSHC_DATPAD_CNFG__WEAKPULL_EN;
	reg16 |= FIELD_PREP(DWCMSHC_DATPAD_CNFG__WEAKPULL_EN, sdio_phy_config->dat_pad[WEAKPULL_EN]);
	reg16 &= ~DWCMSHC_DATPAD_CNFG__RXSEL;
	reg16 |= FIELD_PREP(DWCMSHC_DATPAD_CNFG__RXSEL, sdio_phy_config->dat_pad[RXSEL]);
	sdhci_writew(host, reg16, DWCMSHC_DATPAD_CNFG);

        /* RST PAD configuration */
        reg16 = sdhci_readw(host, DWCMSHC_RSTNPAD_CNFG);
	reg16 &= ~DWCMSHC_RSTNPAD_CNFG__TXSLEW_CTRL_N;
	reg16 |= FIELD_PREP(DWCMSHC_RSTNPAD_CNFG__TXSLEW_CTRL_N, sdio_phy_config->rst_pad[TXSLEW_CTRL_N]);
	reg16 &= ~DWCMSHC_RSTNPAD_CNFG__TXSLEW_CTRL_P;
	reg16 |= FIELD_PREP(DWCMSHC_RSTNPAD_CNFG__TXSLEW_CTRL_P, sdio_phy_config->rst_pad[TXSLEW_CTRL_P]);
	reg16 &= ~DWCMSHC_RSTNPAD_CNFG__WEAKPULL_EN;
	reg16 |= FIELD_PREP(DWCMSHC_RSTNPAD_CNFG__WEAKPULL_EN, sdio_phy_config->rst_pad[WEAKPULL_EN]);
	reg16 &= ~DWCMSHC_RSTNPAD_CNFG__RXSEL;
	reg16 |= FIELD_PREP(DWCMSHC_RSTNPAD_CNFG__RXSEL, sdio_phy_config->rst_pad[RXSEL]);
	sdhci_writew(host, reg16, DWCMSHC_RSTNPAD_CNFG);

        /* CLK PAD configuration */
        reg16 = sdhci_readw(host, DWCMSHC_CLKPAD_CNFG);
	reg16 &= ~DWCMSHC_CLKPAD_CNFG__TXSLEW_CTRL_N;
	reg16 |= FIELD_PREP(DWCMSHC_CLKPAD_CNFG__TXSLEW_CTRL_N, sdio_phy_config->clk_pad[TXSLEW_CTRL_N]);
	reg16 &= ~DWCMSHC_CLKPAD_CNFG__TXSLEW_CTRL_P;
	reg16 |= FIELD_PREP(DWCMSHC_CLKPAD_CNFG__TXSLEW_CTRL_P, sdio_phy_config->clk_pad[TXSLEW_CTRL_P]);
	reg16 &= ~DWCMSHC_CLKPAD_CNFG__WEAKPULL_EN;
	reg16 |= FIELD_PREP(DWCMSHC_CLKPAD_CNFG__WEAKPULL_EN, sdio_phy_config->clk_pad[WEAKPULL_EN]);
	reg16 &= ~DWCMSHC_CLKPAD_CNFG__RXSEL;
	reg16 |= FIELD_PREP(DWCMSHC_CLKPAD_CNFG__RXSEL, sdio_phy_config->clk_pad[RXSEL]);
	sdhci_writew(host, reg16, DWCMSHC_CLKPAD_CNFG);

    	/* wait for phy power good */
    	while (!(sdhci_readw(host, DWCMSHC_PHY_CNFG) & DWCMSHC_PHY_CNFG__PHY_PWRGOOD));

    	/* de-assert phy reset */
    	reg32 = sdhci_readl(host, DWCMSHC_PHY_CNFG);
	reg32 &= ~DWCMSHC_PHY_CNFG__PHY_RSTN;
	reg32 |= DWCMSHC_PHY_CNFG__PHY_RSTN;
	sdhci_writel(host, reg32, DWCMSHC_PHY_CNFG);

    	/* enable pll */
    	reg16 = sdhci_readw(host, DWCMSHC_CLK_CTRL_R);
	reg16 &= ~DWCMSHC_CLK_CTRL_R__PLL_ENABLE;
	reg16 |= DWCMSHC_CLK_CTRL_R__PLL_ENABLE;
	sdhci_writew(host, reg16, DWCMSHC_CLK_CTRL_R);

	/* Adding Clock delay */
	reg8 = sdhci_readb(host, DWCMSHC_SDCLKDL_CNFG);
	reg8 &= ~DWCMSHC_SDCLKDL_CNFG__EXTDLY_EN;
	if (sdio_phy_config->clk_delay[EXTDLY_EN]) {
		reg8 |= DWCMSHC_SDCLKDL_CNFG__EXTDLY_EN;
	}
	sdhci_writeb(host, reg8, DWCMSHC_SDCLKDL_CNFG);

	reg16 = sdhci_readw(host, DWCMSHC_SDCLKDL_DC);
	reg16 &= ~DWCMSHC_SDCLKDL_DC__CCKDL_DC;
	reg16 |= FIELD_PREP(DWCMSHC_SDCLKDL_DC__CCKDL_DC, sdio_phy_config->clk_delay[CCKDL_DC]);
	sdhci_writew(host, reg16, DWCMSHC_SDCLKDL_DC); 


	/* PHY configuration - NMOS, PMOS TX drive strength control */
	reg32 = sdhci_readl(host, DWCMSHC_PHY_CNFG);
	reg32 &= ~DWCMSHC_PHY_CNFG__PAD_SP;
	reg32 |= FIELD_PREP(DWCMSHC_PHY_CNFG__PAD_SP, sdio_phy_config->drive_strength[PAD_SP]);
	reg32 &= ~DWCMSHC_PHY_CNFG__PAD_SN;
	reg32 |= FIELD_PREP(DWCMSHC_PHY_CNFG__PAD_SN, sdio_phy_config->drive_strength[PAD_SN]);
	sdhci_writel(host, reg32, DWCMSHC_PHY_CNFG);
}

static int snps_sdhci_bind(struct udevice *dev)
{
	struct snps_sdhci_plat *plat = dev_get_plat(dev);

	return sdhci_bind(dev, &plat->mmc, &plat->cfg);
}

static int snps_sdhci_setup_clks(struct udevice *dev)
{
	struct sdhci_host *host = dev_get_priv(dev);
	struct snps_sdhci_plat *plat = dev_get_plat(dev);
	unsigned int max_clk;
	int ret = 0;

	ret = clk_get_by_name(dev, "m_clk", &plat->bus_clk);
    	if (ret)
        	return ret;

    	ret = clk_enable(&plat->bus_clk);
    	if (ret) {
		dev_err(dev, "bus clk enable failed: ret[%d]\n", ret);
		goto free_bus_clk;
	}

	ret = clk_get_by_name(dev, "card_clk", &plat->card_clk);
    	if (ret)
        	goto free_bus_clk;

    	ret = clk_enable(&plat->card_clk);
    	if (ret) {
		dev_err(dev, "card clk stable enable failed: ret[%d]\n", ret);
		goto free_card_clk;
	}

	ret = clk_get_by_name(dev, "core", &plat->core_clk);
    	if (ret)
        	goto disable_bus_clk;

	host->max_clk  = clk_get_rate(&plat->core_clk);
    	if (IS_ERR_VALUE(max_clk))
        	goto disable_bus_clk;

	
	ret = clk_get_by_name(dev, "clk_div_bypass", &plat->div_clk_bypass);
    	if (ret)
        	goto disable_bus_clk;

    	ret = clk_disable(&plat->div_clk_bypass);
    	if (ret) {
		dev_err(dev, "clk_divider bypass disable failed: ret[%d]\n", ret);
		goto err;
	}
	plat->is_clk_divider_bypass = false;

	return ret;

err:
	clk_free(&plat->div_clk_bypass);
disable_bus_clk:
	dev_err(dev, "core clk get rate failed for rate %d\n", host->max_clk);
	clk_disable(&plat->bus_clk);
free_card_clk:
	clk_free(&plat->card_clk);
free_bus_clk:
    	clk_free(&plat->bus_clk);

    	return ret;
}
static int snps_sdhci_remove(struct udevice *dev) {
	struct snps_sdhci_plat *plat = dev_get_plat(dev);
	int ret = 0;
	
	if (plat->is_clk_divider_bypass) {
		clk_disable(&plat->div_clk_bypass);
		plat->is_clk_divider_bypass = false;
	}
	
	clk_free(&plat->div_clk_bypass);

	clk_disable(&plat->bus_clk);
	clk_free(&plat->bus_clk);

	clk_disable(&plat->card_clk);
	clk_free(&plat->card_clk);

	return ret;
}

static void hailo_set_clock_dividier(struct sdhci_host *host, unsigned int clock, unsigned int *div) {
	unsigned int calc_div = 0;
	bool is_bypass_needed = (host->max_clk <= clock);
	struct snps_sdhci_plat *plat = dev_get_plat(host->mmc->dev);
	int ret = 0;
	*div = 1023;
	
	/* No need to bypass, need to disable bypass in case its enabled */
	if ((!is_bypass_needed) && (plat->is_clk_divider_bypass)){
		ret = clk_disable(&plat->div_clk_bypass);
    		if (ret) {
			dev_err(host->mmc->dev, "clock divider bypass clr failed: ret[%d]\n", ret);
		}
		plat->is_clk_divider_bypass = false; 
	/* Need to bypass, in case divider bypass is not already enabled */
	} else if ((is_bypass_needed) && (!plat->is_clk_divider_bypass)) {
		ret = clk_enable(&plat->div_clk_bypass);
    		if (ret) {
			dev_err(host->mmc->dev, "clock divider bypass set failed: ret[%d]\n", ret);
			*div = 1;
		}
		plat->is_clk_divider_bypass = true; 	
	}

	for (calc_div = 1; calc_div <= 1023; calc_div++) {
		if ((host->max_clk / calc_div) <= clock) {
			*div = calc_div;
			break;
		}
	}
}

#define BOUNDARY_OK(addr, len) \
	((addr | (SZ_128M - 1)) == ((addr + len - 1) | (SZ_128M - 1)))

void snps_sdhci_adma_desc(struct sdhci_adma_desc **desc,
			    dma_addr_t addr, u16 len, bool end)
{
	int tmplen, offset;

	if (likely(BOUNDARY_OK(addr, len))) {
		sdhci_adma_desc(desc, addr, len, end);
		return;
	}

	offset = addr & (SZ_128M - 1);
	tmplen = SZ_128M - offset;
	sdhci_adma_desc(desc, addr, tmplen, false);

	addr += tmplen;
	len -= tmplen;
	sdhci_adma_desc(desc, addr, len, end);
}

static struct sdhci_ops hailo15_sdhci_ops = {
	.set_clock_dividier = hailo_set_clock_dividier,
	.sdhci_adma_desc = snps_sdhci_adma_desc,
};

static int snps_sdhci_probe(struct udevice *dev)
{
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct snps_sdhci_plat *plat = dev_get_plat(dev);
	struct sdhci_host *host = dev_get_priv(dev);
	fdt_addr_t base;
	int ret;
	unsigned int mux_mod;
	ofnode phy_config_node;

	base = devfdt_get_addr(dev);
	if (base == FDT_ADDR_T_NONE)
		return -EINVAL;

	host->name = dev->name;
	host->ioaddr = (void *)base;

	ret = snps_sdhci_setup_clks(dev);
	if (ret)
		return ret;

	ret = mmc_of_parse(dev, &plat->cfg);
	if (ret)
		return ret;

	host->mmc = &plat->mmc;
	host->mmc->dev = dev;

	host->ops = &hailo15_sdhci_ops;
	/**
	 * descriptor address + len must not cross 128MB interval
	 * therefore need to add max times that 1 mmc read can corss this interval
	 */
	host->adma_desc_table_extra_desc = DIV_ROUND_UP(MMC_MAX_BYTES_READ, SZ_128M);

	ret = sdhci_setup_cfg(&plat->cfg, host,
			      0,
				  (host->max_clk / 1023));
	if (ret)
		return ret;

	ret = reset_get_by_index(dev, 0, &plat->reset);
	if (ret) {
		dev_err(dev, "failed to get 8-bit mux reset: ret[%d]\n", ret);
		return ret;
	}

	ret = dev_read_u32(dev, "switch-mode", &mux_mod);
	if (ret) {
		dev_err(dev, "failed to get 8-bit mux mode: ret[%d]\n", ret);
		return ret;
	}

	if(mux_mod == 8) {
		// Reset -Set 8 bit mux in case this is 8bit
		ret = reset_deassert(&plat->reset);
	}
	else{
		// Reset -Clr 8 bit mux in case this is 4bit
		ret = reset_assert(&plat->reset);
	}
        if (ret) {
		dev_err(dev, "reset mux failed: ret[%d]\n", ret);
		return ret;
	}

	if (device_is_compatible(dev, "hailo,dwcmshc-sdhci-0")) {
		// Set PIO and max block count to 1, will be removed as part of MSW-1429
		host->flags &= ~(USE_DMA);
	}
	upriv->mmc = &plat->mmc;
	host->mmc->priv = host;

	ret = sdhci_probe(dev);
	
	phy_config_node = dev_read_subnode(dev, "phy-config");
	if (!ofnode_valid(phy_config_node))
		return -FDT_ERR_NOTFOUND;
		
	ofnode_read_u32(phy_config_node, "card-is-emmc", &plat->sdio_phy_config.card_is_emmc);
	ofnode_read_u32_array(phy_config_node, "cmd-pad-values", plat->sdio_phy_config.cmd_pad, PAD_CONFIG_MAX);
	ofnode_read_u32_array(phy_config_node, "dat-pad-values", plat->sdio_phy_config.dat_pad, PAD_CONFIG_MAX);
	ofnode_read_u32_array(phy_config_node, "rst-pad-values", plat->sdio_phy_config.rst_pad, PAD_CONFIG_MAX);
	ofnode_read_u32_array(phy_config_node, "clk-pad-values", plat->sdio_phy_config.clk_pad, PAD_CONFIG_MAX);
	ofnode_read_u32_array(phy_config_node, "sdclkdl-cnfg", plat->sdio_phy_config.clk_delay, CLK_DELAY_CONFIG_MAX);
	ofnode_read_u32_array(phy_config_node, "drive-strength", plat->sdio_phy_config.drive_strength, DS_CONFIG_MAX);
	sdhci_hailo15_phy_config(host, dev, &plat->sdio_phy_config);
	dev_info(dev, "phy configuration for %s mode done\n", plat->sdio_phy_config.card_is_emmc ? "EMMC ": "SD");

	return ret;
}

static const struct udevice_id snps_sdhci_match[] = {
	{ .compatible = "hailo,dwcmshc-sdhci-1" },
	{ .compatible = "hailo,dwcmshc-sdhci-0" },
	{}
};

U_BOOT_DRIVER(snps_sdhci) = {
	.name = "snps_sdhci",
	.id = UCLASS_MMC,
	.of_match = snps_sdhci_match,
	.ops = &sdhci_ops,
	.bind = snps_sdhci_bind,
	.probe = snps_sdhci_probe,
	.remove	= snps_sdhci_remove,
    	.priv_auto = sizeof(struct sdhci_host),
    	.plat_auto = sizeof(struct snps_sdhci_plat),
};
