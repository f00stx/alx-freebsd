/*-
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * Copyright (c) 2013, Mark Johnston <markj@FreeBSD.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/taskqueue.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_vlan_var.h>

#include <netinet/in.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <machine/bus.h>

#include "compat.h"
#include "alx_hw.h"
#include "if_alxreg.h"
#include "if_alxvar.h"

MODULE_DEPEND(alx, pci, 1, 1, 1);
MODULE_DEPEND(alx, ether, 1, 1, 1);

#define DRV_MAJ		1
#define DRV_MIN		2
#define DRV_PATCH	3
#define DRV_MODULE_VER	\
	__stringify(DRV_MAJ) "." __stringify(DRV_MIN) "." __stringify(DRV_PATCH)

static struct alx_dev {
	uint16_t	 alx_vendorid;
	uint16_t	 alx_deviceid;
	const char	*alx_name;
} alx_devs[] = {
	{ ALX_VENDOR_ID, ALX_DEV_ID_AR8161,
	    "Qualcomm Atheros AR8161 Gigabit Ethernet" },
	{ ALX_VENDOR_ID, ALX_DEV_ID_AR8162,
	    "Qualcomm Atheros AR8162 Fast Ethernet" },
	{ ALX_VENDOR_ID, ALX_DEV_ID_AR8171,
	    "Qualcomm Atheros AR8171 Gigabit Ethernet" },
	{ ALX_VENDOR_ID, ALX_DEV_ID_AR8172,
	    "Qualcomm Atheros AR8172 Fast Ethernet" },
};

#define ALX_DEV_COUNT	(sizeof(alx_devs) / sizeof(alx_devs[0]))

static int	alx_allocate_legacy_irq(struct alx_softc *);
static int	alx_attach(device_t);
static int	alx_detach(device_t);
static int	alx_dma_alloc(struct alx_softc *);
static void	alx_dma_free(struct alx_softc *);
static void	alx_dmamap_cb(void *, bus_dma_segment_t *, int, int);
static int	alx_encap(struct alx_softc *, struct mbuf **);
static int	alx_ioctl(struct ifnet *, u_long, caddr_t);
static void	alx_init(void *);
static void	alx_init_locked(struct alx_softc *);
static void	alx_init_tx_ring(struct alx_softc *);
static void	alx_int_task(void *, int);
static void	alx_intr_disable(struct alx_softc *);
static void	alx_intr_enable(struct alx_softc *);
static int	alx_irq_legacy(void *);
static int	alx_media_change(struct ifnet *);
static void	alx_media_status(struct ifnet *, struct ifmediareq *);
static int	alx_probe(device_t);
static void	alx_reset(struct alx_softc *sc);
static int	alx_resume(device_t);
static int	alx_shutdown(device_t);
static void	alx_start(struct ifnet *);
static void	alx_start_locked(struct ifnet *);
static void	alx_stop(struct alx_softc *);
static int	alx_suspend(device_t);

static device_method_t alx_methods[] = {
	DEVMETHOD(device_probe,		alx_probe),
	DEVMETHOD(device_attach,	alx_attach),
	DEVMETHOD(device_detach,	alx_detach),
	DEVMETHOD(device_shutdown,	alx_shutdown),
	DEVMETHOD(device_suspend,	alx_suspend),
	DEVMETHOD(device_resume,	alx_resume),

	DEVMETHOD_END
};

static driver_t alx_driver = {
	"alx",
	alx_methods,
	sizeof(struct alx_softc)
};

static devclass_t alx_devclass;

DRIVER_MODULE(alx, pci, alx_driver, alx_devclass, 0, 0);

#if 0
static int alx_poll(struct napi_struct *napi, int budget);
static irqreturn_t alx_msix_ring(int irq, void *data);
static irqreturn_t alx_intr_msix_misc(int irq, void *data);
static irqreturn_t alx_intr_msi(int irq, void *data);
static irqreturn_t alx_intr_legacy(int irq, void *data);
static void alx_init_ring_ptrs(struct alx_adapter *adpt);
static int alx_reinit_rings(struct alx_adapter *adpt);

static inline void alx_schedule_work(struct alx_adapter *adpt)
{
	if (!ALX_FLAG(adpt, HALT))
		schedule_work(&adpt->task);
}

static inline void alx_cancel_work(struct alx_adapter *adpt)
{
	cancel_work_sync(&adpt->task);
}


static void __alx_set_rx_mode(struct net_device *netdev)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	struct alx_hw *hw = &adpt->hw;
	struct netdev_hw_addr *ha;


	/* comoute mc addresses' hash value ,and put it into hash table */
	netdev_for_each_mc_addr(ha, netdev)
		alx_add_mc_addr(hw, ha->addr);

	ALX_MEM_W32(hw, ALX_HASH_TBL0, hw->mc_hash[0]);
	ALX_MEM_W32(hw, ALX_HASH_TBL1, hw->mc_hash[1]);

	/* check for Promiscuous and All Multicast modes */
	hw->rx_ctrl &= ~(ALX_MAC_CTRL_MULTIALL_EN | ALX_MAC_CTRL_PROMISC_EN);
	if (netdev->flags & IFF_PROMISC)
		hw->rx_ctrl |= ALX_MAC_CTRL_PROMISC_EN;
	if (netdev->flags & IFF_ALLMULTI)
		hw->rx_ctrl |= ALX_MAC_CTRL_MULTIALL_EN;

	ALX_MEM_W32(hw, ALX_MAC_CTRL, hw->rx_ctrl);
}

/* alx_set_rx_mode - Multicast and Promiscuous mode set */
static void alx_set_rx_mode(struct net_device *netdev)
{
	__alx_set_rx_mode(netdev);
}


/* alx_set_mac - Change the Ethernet Address of the NIC */
static int alx_set_mac_address(struct net_device *netdev, void *data)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	struct alx_hw *hw = &adpt->hw;
	struct sockaddr *addr = data;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	if (netdev->addr_assign_type & NET_ADDR_RANDOM)
		netdev->addr_assign_type ^= NET_ADDR_RANDOM;

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	memcpy(hw->mac_addr, addr->sa_data, netdev->addr_len);
	alx_set_macaddr(hw, hw->mac_addr);

	return 0;
}

static void alx_free_napis(struct alx_adapter *adpt)
{
	struct alx_napi *np;
	int i;

	for (i = 0; i < adpt->nr_napi; i++) {
		np = adpt->qnapi[i];
		if (!np)
			continue;

		netif_napi_del(&np->napi);
		kfree(np->txq);
		np->txq = NULL;
		kfree(np->rxq);
		np->rxq = NULL;
		adpt->qnapi[i] = NULL;
	}
}
static u16 tx_pidx_reg[] = {ALX_TPD_PRI0_PIDX, ALX_TPD_PRI1_PIDX,
			    ALX_TPD_PRI2_PIDX, ALX_TPD_PRI3_PIDX};
static u16 tx_cidx_reg[] = {ALX_TPD_PRI0_CIDX, ALX_TPD_PRI1_CIDX,
			    ALX_TPD_PRI2_CIDX, ALX_TPD_PRI3_CIDX};
static u32 tx_vect_mask[] = {ALX_ISR_TX_Q0, ALX_ISR_TX_Q1,
			     ALX_ISR_TX_Q2, ALX_ISR_TX_Q3};
static u32 rx_vect_mask[] = {ALX_ISR_RX_Q0, ALX_ISR_RX_Q1,
			     ALX_ISR_RX_Q2, ALX_ISR_RX_Q3,
			     ALX_ISR_RX_Q4, ALX_ISR_RX_Q5,
			     ALX_ISR_RX_Q6, ALX_ISR_RX_Q7};
static int alx_alloc_napis(struct alx_adapter *adpt)
{
	struct alx_hw		*hw = &adpt->hw;
	struct alx_napi		*np;
	struct alx_rx_queue	*rxq;
	struct alx_tx_queue	*txq;
	int i;

	hw->imask &= ~ALX_ISR_ALL_QUEUES;

	/* alloc alx_napi */
	for (i = 0; i < adpt->nr_napi; i++) {
		np = kzalloc(sizeof(struct alx_napi), GFP_KERNEL);
		if (!np)
			goto err_out;

		np->adpt = adpt;
		netif_napi_add(adpt->netdev, &np->napi, alx_poll, 64);
		adpt->qnapi[i] = np;
	}

	/* alloc tx queue */
	for (i = 0; i < adpt->nr_txq; i++) {
		np = adpt->qnapi[i];
		txq = kzalloc(sizeof(struct alx_tx_queue), GFP_KERNEL);
		if (!txq)
			goto err_out;
		np->txq = txq;
		txq->p_reg = tx_pidx_reg[i];
		txq->c_reg = tx_cidx_reg[i];
		txq->count = adpt->tx_ringsz;
		txq->qidx = (u16)i;
		np->vec_mask |= tx_vect_mask[i];
		hw->imask |= tx_vect_mask[i];
	}

	/* alloc rx queue */
	for (i = 0; i < adpt->nr_rxq; i++) {
		np = adpt->qnapi[i];
		rxq = kzalloc(sizeof(struct alx_rx_queue), GFP_KERNEL);
		if (!rxq)
			goto err_out;
		np->rxq = rxq;
		rxq->p_reg = ALX_RFD_PIDX;
		rxq->c_reg = ALX_RFD_CIDX;
		rxq->count = adpt->rx_ringsz;
		rxq->qidx = (u16)i;
		__skb_queue_head_init(&rxq->list);
		np->vec_mask |= rx_vect_mask[i];
		hw->imask |= rx_vect_mask[i];
	}

	return 0;

err_out:
	alx_free_napis(adpt);
	return -ENOMEM;
}

#endif

static void
alx_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{

	if (error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

/*
 * XXX:
 * - multiple queues
 * - does the chipset's DMA engine support more than one segment?
 */
static int
alx_dma_alloc(struct alx_softc *sc)
{
	device_t dev;
	struct alx_hw *hw;
	int error;

	dev = sc->alx_dev;
	hw = &sc->hw;

	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->alx_dev),	/* parent */
	    1, 0,				/* alignment, boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    BUS_SPACE_MAXSIZE_32BIT,		/* maxsize */
	    1,					/* nsegments */
	    BUS_SPACE_MAXSIZE_32BIT,		/* maxsegsize */
	    0,					/* flags */
	    NULL, NULL,				/* lockfunc, lockfuncarg */
	    &sc->alx_parent_tag);
	if (error != 0) {
		device_printf(dev, "could not create parent DMA tag\n");
		return (error);
	}

	/* Create the DMA tag for the transmit packet descriptor ring. */
	/* XXX assuming 1 queue at the moment. */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),		/* parent */
	    8, 0,				/* alignment, boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    sc->tx_ringsz * sizeof(struct tpd_desc), /* maxsize */
	    1,					/* nsegments */
	    sc->tx_ringsz * sizeof(struct tpd_desc), /* maxsegsize */
	    0,					/* flags */
	    NULL, NULL,				/* lockfunc, lockfuncarg */
	    &sc->alx_tx_tag);
	if (error != 0) {
		device_printf(dev, "could not create TX descriptor ring tag\n");
		return (error);
	}

	/* Allocate DMA memory for the transmit packet descriptor ring. */
	error = bus_dmamem_alloc(sc->alx_tx_tag,
	    (void **)&sc->alx_tx_queue.tpd_hdr,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO | BUS_DMA_COHERENT,
	    &sc->alx_tx_dmamap);
	if (error != 0) {
		device_printf(dev,
		    "could not allocate DMA'able memory for TX ring\n");
		/* XXX cleanup */
		return (error);
	}

	/* Do the actual DMA mapping of the transmit packet descriptor ring. */
	error = bus_dmamap_load(sc->alx_tx_tag, sc->alx_tx_dmamap,
	    sc->alx_tx_queue.tpd_hdr, sc->tx_ringsz * sizeof(struct tpd_desc),
	    alx_dmamap_cb, &sc->alx_tx_queue.tpd_dma, 0);
	if (error != 0) {
		device_printf(dev, "could not load DMA map for TX ring\n");
		/* XXX cleanup */
		return (error);
	}

	/* Create the DMA tag for the receive ready descriptor ring. */
	/* XXX assuming 1 queue at the moment. */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),		/* parent */
	    8, 0,				/* alignment, boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    sc->rx_ringsz * sizeof(struct rrd_desc), /* maxsize */
	    1,					/* nsegments */
	    sc->rx_ringsz * sizeof(struct rrd_desc), /* maxsegsize */
	    0,					/* flags */
	    NULL, NULL,				/* lockfunc, lockfuncarg */
	    &sc->alx_rr_tag);
	if (error != 0) {
		device_printf(dev, "could not create RX descriptor ring tag\n");
		return (error);
	}

	error = bus_dmamem_alloc(sc->alx_rr_tag,
	    (void **)&sc->alx_rx_queue.rrd_hdr,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO | BUS_DMA_COHERENT,
	    &sc->alx_rr_dmamap);
	if (error != 0) {
		device_printf(dev,
		    "could not allocate DMA'able memory for RX ring\n");
		/* XXX cleanup */
		return (error);
	}

	error = bus_dmamap_load(sc->alx_rr_tag, sc->alx_rr_dmamap,
	    sc->alx_rx_queue.rrd_hdr, sc->rx_ringsz * sizeof(struct rrd_desc),
	    alx_dmamap_cb, &sc->alx_rx_queue.rrd_dma, 0);
	if (error != 0) {
		device_printf(dev,
		    "could not load DMA map for RX ready ring\n");
		/* XXX cleanup */
		return (error);
	}

	/* Create the DMA tag for the receive ready descriptor ring. */
	/* XXX assuming 1 queue at the moment. */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),		/* parent */
	    8, 0,				/* alignment, boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    sc->rx_ringsz * sizeof(struct rfd_desc), /* maxsize */
	    1,					/* nsegments */
	    sc->rx_ringsz * sizeof(struct rfd_desc), /* maxsegsize */
	    0,					/* flags */
	    NULL, NULL,				/* lockfunc, lockfuncarg */
	    &sc->alx_rx_tag);
	if (error != 0) {
		device_printf(dev, "could not create RX descriptor ring tag\n");
		return (error);
	}

	error = bus_dmamem_alloc(sc->alx_rx_tag,
	    (void **)&sc->alx_rx_queue.rfd_hdr,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO | BUS_DMA_COHERENT,
	    &sc->alx_rx_dmamap);
	if (error != 0) {
		device_printf(dev,
		    "could not allocate DMA'able memory for RX ring\n");
		/* XXX cleanup */
		return (error);
	}

	error = bus_dmamap_load(sc->alx_rx_tag, sc->alx_rx_dmamap,
	    sc->alx_rx_queue.rfd_hdr, sc->rx_ringsz * sizeof(struct rfd_desc),
	    alx_dmamap_cb, &sc->alx_rx_queue.rfd_dma, 0);
	if (error != 0) {
		device_printf(dev, "could not load DMA map for RX free ring\n");
		/* XXX cleanup */
		return (error);
	}

	return (error);
}

static void __unused
alx_dma_free(struct alx_softc *sc)
{

}

#if 0
static int
alx_alloc_rings(struct alx_softc *sc)
{
	struct alx_buffer *bf;
	u8 *desc;
	dma_addr_t  dma;
	int i, size, offset;

	size = sizeof(struct alx_buffer) * adpt->nr_txq * adpt->tx_ringsz +
	       sizeof(struct alx_buffer) * adpt->nr_hwrxq * adpt->rx_ringsz;

	bf = vzalloc(size);
	if (!bf)
		goto err_out;

	/* physical rx rings */
	size = sizeof(struct tpd_desc) * adpt->tx_ringsz * adpt->nr_txq +
	       (sizeof(struct rrd_desc) + sizeof(struct rfd_desc)) *
	       adpt->rx_ringsz * adpt->nr_hwrxq +
	       adpt->nr_txq * 8 +
	       adpt->nr_hwrxq * 8;
	desc = dma_alloc_coherent(&adpt->pdev->dev, size, &dma, GFP_KERNEL);
	if (!desc)
		goto err_out;

	memset(desc, 0, size);
	adpt->ring_header.desc = desc;
	adpt->ring_header.dma = dma;
	adpt->ring_header.size = size;

	size = sizeof(struct tpd_desc) * adpt->tx_ringsz;
	for (i = 0; i < adpt->nr_txq; i++) {
		offset = ALIGN(dma, 8) - dma;
		desc += offset;
		dma += offset;
		adpt->qnapi[i]->txq->netdev = adpt->netdev;
		adpt->qnapi[i]->txq->dev = &adpt->pdev->dev;
		adpt->qnapi[i]->txq->tpd_hdr = (struct tpd_desc *)desc;
		adpt->qnapi[i]->txq->tpd_dma = dma;
		adpt->qnapi[i]->txq->count = adpt->tx_ringsz;
		adpt->qnapi[i]->txq->bf_info = bf;
		desc += size;
		dma += size;
		bf += adpt->tx_ringsz;
	}
	size = sizeof(struct rrd_desc) * adpt->rx_ringsz;
	for (i = 0; i < adpt->nr_hwrxq; i++) {
		offset = ALIGN(dma, 8) - dma;
		desc += offset;
		dma += offset;
		adpt->qnapi[i]->rxq->rrd_hdr = (struct rrd_desc *)desc;
		adpt->qnapi[i]->rxq->rrd_dma = dma;
		adpt->qnapi[i]->rxq->bf_info = bf;
		desc += size;
		dma += size;
		bf += adpt->rx_ringsz;
	}
	size = sizeof(struct rfd_desc) * adpt->rx_ringsz;
	for (i = 0; i < adpt->nr_hwrxq; i++) {
		offset = ALIGN(dma, 8) - dma;
		desc += offset;
		dma += offset;
		adpt->qnapi[i]->rxq->rfd_hdr = (struct rfd_desc *)desc;
		adpt->qnapi[i]->rxq->rfd_dma = dma;
		desc += size;
		dma += size;
	}
	for (i = 0; i < adpt->nr_rxq; i++) {
		adpt->qnapi[i]->rxq->netdev = adpt->netdev;
		adpt->qnapi[i]->rxq->dev = &adpt->pdev->dev;
		adpt->qnapi[i]->rxq->count = adpt->rx_ringsz;
	}

	return 0;

err_out:
	if (bf)
		vfree(bf);

	return -ENOMEM;
}

static void alx_free_rings(struct alx_adapter *adpt)
{
	struct alx_buffer *bf;
	struct alx_napi *np;

	/* alx_buffer header is in the 1st tpdq->bf_info */
	np = adpt->qnapi[0];
	if (np) {
		bf = np->txq->bf_info;
		if (bf) {
			vfree(bf);
			np->txq->bf_info = NULL;
		}
	}
	if (adpt->ring_header.desc) {
		dma_free_coherent(&adpt->pdev->dev,
				  adpt->ring_header.size,
				  adpt->ring_header.desc,
				  adpt->ring_header.dma);
		adpt->ring_header.desc = NULL;
	}
}

/* dequeue skb from RXQ, return true if the RXQ is empty */
static inline bool alx_skb_dequeue_n(struct alx_rx_queue *rxq, int max_pkts,
				     struct sk_buff_head *list)
{
	struct alx_adapter *adpt = netdev_priv(rxq->netdev);
	bool use_lock = !ALX_CAP(&adpt->hw, MRQ);
	bool empty;
	struct sk_buff *skb;
	int count = 0;

	if (use_lock)
		spin_lock(&rxq->list.lock);

	while (count < max_pkts || max_pkts == -1) {
		skb = __skb_dequeue(&rxq->list);
		if (skb) {
			__skb_queue_tail(list, skb);
			count++;
		} else
			break;
	}

	empty = skb_queue_empty(&rxq->list);

	if (use_lock)
		spin_unlock(&rxq->list.lock);

	netif_info(adpt, rx_status, adpt->netdev,
		   "RX %d packets\n",
		   count);

	return empty;
}

static inline void alx_skb_queue_tail(struct alx_rx_queue *rxq,
				      struct sk_buff *skb)
{
	struct alx_adapter *adpt = netdev_priv(rxq->netdev);
	bool use_lock = !ALX_CAP(&adpt->hw, MRQ);

	if (use_lock)
		spin_lock(&rxq->list.lock);

	__skb_queue_tail(&rxq->list, skb);

	if (use_lock)
		spin_unlock(&rxq->list.lock);
}

int alx_alloc_rxring_buf(struct alx_adapter *adpt,
			 struct alx_rx_queue *rxq)
{
	struct sk_buff *skb;
	struct alx_buffer *cur_buf;
	struct rfd_desc *rfd;
	dma_addr_t dma;
	u16 cur, next, count = 0;

	next = cur = rxq->pidx;
	if (++next == rxq->count)
		next = 0;
	cur_buf = rxq->bf_info + cur;
	rfd = rxq->rfd_hdr + cur;

	while (!cur_buf->skb && next != rxq->cidx) {
		skb = dev_alloc_skb(adpt->rxbuf_size);
		if (unlikely(!skb)) {
			netdev_warn(adpt->netdev, "alloc skb failed\n");
			break;
		}
		dma = dma_map_single(rxq->dev,
				     skb->data,
				     adpt->rxbuf_size,
				     DMA_FROM_DEVICE);
		if (dma_mapping_error(rxq->dev, dma)) {
			netdev_warn(adpt->netdev, "mapping rx-buffer failed\n");
			dev_kfree_skb(skb);
			break;
		}
		cur_buf->skb = skb;
		dma_unmap_len_set(cur_buf, size, adpt->rxbuf_size);
		dma_unmap_addr_set(cur_buf, dma, dma);
		rfd->addr = cpu_to_le64(dma);

		cur = next;
		if (++next == rxq->count)
			next = 0;
		cur_buf = rxq->bf_info + cur;
		rfd = rxq->rfd_hdr + cur;
		count++;
	}

	if (count) {
		wmb();
		rxq->pidx = cur;
		ALX_MEM_W16(&adpt->hw, rxq->p_reg, (u16)cur);
	}

	return count;
}

static void alx_free_rxring_buf(struct alx_rx_queue *rxq)
{
	struct alx_buffer *cur_buf;
	struct sk_buff_head list;
	u16 i;

	if (rxq == NULL)
		return;

	for (i = 0; i < rxq->count; i++) {
		cur_buf = rxq->bf_info + i;
		if (cur_buf->skb) {
			dma_unmap_single(rxq->dev,
					 dma_unmap_addr(cur_buf, dma),
					 dma_unmap_len(cur_buf, size),
					 DMA_FROM_DEVICE);
			dev_kfree_skb(cur_buf->skb);
			cur_buf->skb = NULL;
			dma_unmap_len_set(cur_buf, size, 0);
			dma_unmap_addr_set(cur_buf, dma, 0);
		}
	}

	/* some skbs might be pending in the list */
	__skb_queue_head_init(&list);
	alx_skb_dequeue_n(rxq, -1, &list);
	while (!skb_queue_empty(&list)) {
		struct sk_buff *skb;

		skb = __skb_dequeue(&list);
		dev_kfree_skb(skb);
	}

	rxq->pidx = 0;
	rxq->cidx = 0;
	rxq->rrd_cidx = 0;
}

int
alx_setup_all_ring_resources(struct alx_softc *sc)
{
	int err;

	err = alx_alloc_rings(sc);
	if (err)
		goto out;

	err = alx_reinit_rings(sc);

out:
	if (err != 0)
		device_printf(sc->alx_dev, "failed to set up TX/RX rings\n");

	return (err);
}

static void alx_txbuf_unmap_and_free(struct alx_tx_queue *txq, int entry)
{
	struct alx_buffer *txb = txq->bf_info + entry;

	if (dma_unmap_len(txb, size) &&
	    txb->flags & ALX_BUF_TX_FIRSTFRAG) {
		dma_unmap_single(txq->dev,
				 dma_unmap_addr(txb, dma),
				 dma_unmap_len(txb, size),
				 DMA_TO_DEVICE);
		txb->flags &= ~ALX_BUF_TX_FIRSTFRAG;
	} else if (dma_unmap_len(txb, size)) {
		dma_unmap_page(txq->dev,
			       dma_unmap_addr(txb, dma),
			       dma_unmap_len(txb, size),
			       DMA_TO_DEVICE);
	}
	if (txb->skb) {
		dev_kfree_skb_any(txb->skb);
		txb->skb = NULL;
	}
	dma_unmap_len_set(txb, size, 0);
}

static void alx_free_txring_buf(struct alx_tx_queue *txq)
{
	int i;

	if (!txq->bf_info)
		return;

	for (i = 0; i < txq->count; i++)
		alx_txbuf_unmap_and_free(txq, i);

	memset(txq->bf_info, 0, txq->count * sizeof(struct alx_buffer));
	memset(txq->tpd_hdr, 0, txq->count * sizeof(struct tpd_desc));
	txq->pidx = 0;
	atomic_set(&txq->cidx, 0);

	netdev_tx_reset_queue(netdev_get_tx_queue(txq->netdev, txq->qidx));
}

/* free up pending skb for tx/rx */
static void alx_free_all_rings_buf(struct alx_adapter *adpt)
{
	int i;

	for (i = 0; i < adpt->nr_txq; i++)
		if (adpt->qnapi[i])
			alx_free_txring_buf(adpt->qnapi[i]->txq);

	for (i = 0; i < adpt->nr_hwrxq; i++)
		if (adpt->qnapi[i])
			alx_free_rxring_buf(adpt->qnapi[i]->rxq);
}

void alx_free_all_ring_resources(struct alx_adapter *adpt)
{
	alx_free_all_rings_buf(adpt);
	alx_free_rings(adpt);
	alx_free_napis(adpt);
}

static inline int alx_tpd_avail(struct alx_tx_queue *txq)
{
	u16 cidx = atomic_read(&txq->cidx);

	return txq->pidx >= cidx ?
		txq->count + cidx - txq->pidx - 1 :
		cidx - txq->pidx - 1;
}



static bool alx_clean_tx_irq(struct alx_tx_queue *txq)
{
	struct alx_adapter *adpt = netdev_priv(txq->netdev);
	struct netdev_queue *netque;
	u16 hw_cidx, sw_cidx;
	unsigned int total_bytes = 0, total_packets = 0;
	int budget = ALX_DEFAULT_TX_WORK;

	if (ALX_FLAG(adpt, HALT))
		return true;

	netque = netdev_get_tx_queue(adpt->netdev, txq->qidx);
	sw_cidx = atomic_read(&txq->cidx);

	ALX_MEM_R16(&adpt->hw, txq->c_reg, &hw_cidx);

	if (sw_cidx != hw_cidx) {

		netif_info(adpt, tx_done, adpt->netdev,
			   "TX[Q:%d, Preg:%x]: cons = 0x%x, hw-cons = 0x%x\n",
			   txq->qidx, txq->p_reg, sw_cidx, hw_cidx);

		while (sw_cidx != hw_cidx && budget > 0) {
			struct sk_buff *skb;

			skb = txq->bf_info[sw_cidx].skb;
			if (skb) {
				total_bytes += skb->len;
				total_packets++;
				budget--;
			}
			alx_txbuf_unmap_and_free(txq, sw_cidx);
			if (++sw_cidx == txq->count)
				sw_cidx = 0;
		}
		atomic_set(&txq->cidx, sw_cidx);

		netdev_tx_completed_queue(netque, total_packets, total_bytes);
	}

	if (unlikely(netif_tx_queue_stopped(netque) &&
		     netif_carrier_ok(adpt->netdev) &&
		     alx_tpd_avail(txq) > ALX_TX_WAKEUP_THRESH(txq) &&
		     !ALX_FLAG(adpt, HALT))) {
		netif_tx_wake_queue(netque);
	}

	return sw_cidx == hw_cidx;
}

static bool alx_dispatch_skb(struct alx_rx_queue *rxq)
{
	struct alx_adapter *adpt = netdev_priv(rxq->netdev);
	struct rrd_desc *rrd;
	struct alx_buffer *rxb;
	struct sk_buff *skb;
	u16 length, rfd_cleaned = 0;
	struct alx_rx_queue *tmp_rxq;
	int qnum;

	if (test_and_set_bit(ALX_RQ_USING, &rxq->flag))
		return false;

	while (1) {
		rrd = rxq->rrd_hdr + rxq->rrd_cidx;
		if (!(rrd->word3 & (1 << RRD_UPDATED_SHIFT)))
			break;
		rrd->word3 &= ~(1 << RRD_UPDATED_SHIFT);

		if (unlikely(FIELD_GETX(rrd->word0, RRD_SI) != rxq->cidx ||
			     FIELD_GETX(rrd->word0, RRD_NOR) != 1)) {
			netif_err(adpt, rx_err, adpt->netdev,
				  "wrong SI/NOR packet! rrd->word0= %08x\n",
				  rrd->word0);
			/* reset chip */
			ALX_FLAG_SET(adpt, TASK_RESET);
			alx_schedule_work(adpt);
			return true;
		}
		rxb = rxq->bf_info + rxq->cidx;
		dma_unmap_single(rxq->dev,
				 dma_unmap_addr(rxb, dma),
				 dma_unmap_len(rxb, size),
				 DMA_FROM_DEVICE);
		dma_unmap_len_set(rxb, size, 0);
		skb = rxb->skb;
		rxb->skb = NULL;

		if (unlikely(rrd->word3 & (1 << RRD_ERR_RES_SHIFT) ||
			     rrd->word3 & (1 << RRD_ERR_LEN_SHIFT))) {
			netdev_warn(adpt->netdev,
				   "wrong packet! rrd->word3 is %08x\n",
				   rrd->word3);
			rrd->word3 = 0;
			dev_kfree_skb_any(skb);
			goto next_pkt;
		}
		length = FIELD_GETX(rrd->word3, RRD_PKTLEN) - ETH_FCS_LEN;
		skb_put(skb, length);
		skb->protocol = eth_type_trans(skb, adpt->netdev);
		/* checksum */
		skb_checksum_none_assert(skb);
		if (adpt->netdev->features & NETIF_F_RXCSUM) {
			switch (FIELD_GETX(rrd->word2, RRD_PID)) {
			case RRD_PID_IPV6UDP:
			case RRD_PID_IPV4UDP:
			case RRD_PID_IPV4TCP:
			case RRD_PID_IPV6TCP:
				if (rrd->word3 & ((1 << RRD_ERR_L4_SHIFT) |
						  (1 << RRD_ERR_IPV4_SHIFT))) {
					netdev_warn(
						adpt->netdev,
						"rx-chksum error, w2=%X\n",
						rrd->word2);
					break;
				}
				skb->ip_summed = CHECKSUM_UNNECESSARY;
				break;
			}
		}
		/* vlan tag */
		if (rrd->word3 & (1 << RRD_VLTAGGED_SHIFT)) {
			u16 tag = ntohs(FIELD_GETX(rrd->word2, RRD_VLTAG));
			__vlan_hwaccel_put_tag(skb, ntohs(tag));
		}
		qnum = FIELD_GETX(rrd->word2, RRD_RSSQ) % adpt->nr_rxq;
		tmp_rxq = ALX_CAP(&adpt->hw, MRQ) ?
				rxq : adpt->qnapi[qnum]->rxq;
		alx_skb_queue_tail(tmp_rxq, skb);

next_pkt:

		if (++rxq->cidx == rxq->count)
			rxq->cidx = 0;
		if (++rxq->rrd_cidx == rxq->count)
			rxq->rrd_cidx = 0;

		if (++rfd_cleaned > ALX_RX_ALLOC_THRESH)
			rfd_cleaned -= alx_alloc_rxring_buf(adpt, rxq);
	}

	if (rfd_cleaned)
		alx_alloc_rxring_buf(adpt, rxq);

	clear_bit(ALX_RQ_USING, &rxq->flag);

	return true;
}

static inline struct napi_struct *alx_rxq_to_napi(
	struct alx_rx_queue *rxq)
{
	struct alx_adapter *adpt = netdev_priv(rxq->netdev);

	return &adpt->qnapi[rxq->qidx]->napi;
}

static bool alx_clean_rx_irq(struct alx_rx_queue *rxq, int budget)
{
	struct sk_buff_head list;
	bool empty;

	__skb_queue_head_init(&list);
	alx_dispatch_skb(alx_hw_rxq(rxq));
	empty = alx_skb_dequeue_n(rxq, budget, &list);
	if (!skb_queue_empty(&list)) {
		struct napi_struct *napi;
		struct sk_buff *skb;

		napi = alx_rxq_to_napi(rxq);
		while (!skb_queue_empty(&list)) {
			skb = __skb_dequeue(&list);
			napi_gro_receive(napi, skb);
		}
	} else {
		struct alx_adapter *adpt = netdev_priv(rxq->netdev);

		netif_info(adpt, rx_status, adpt->netdev,
			   "no packet received for this rxQ\n");
	}


	return empty;
}

static int alx_request_msix(struct alx_adapter *adpt)
{
	struct net_device *netdev = adpt->netdev;
	int i, err;
	int vec;

	err = request_irq(adpt->msix_ent[0].vector,
		alx_intr_msix_misc, 0, netdev->name, adpt);
	if (err)
		goto out;

	vec = 1;
	for (i = 0; i < adpt->nr_napi; i++) {
		struct alx_napi *np = adpt->qnapi[i];

		if (np->txq && np->rxq)
			sprintf(np->irq_lbl, "%s-TR-%u", netdev->name, i);
		else if (np->txq)
			sprintf(np->irq_lbl, "%s-T-%u", netdev->name, i);
		else
			sprintf(np->irq_lbl, "%s-R-%u", netdev->name, i);

		np->vec_idx = vec;
		err = request_irq(adpt->msix_ent[vec].vector,
			alx_msix_ring, 0, np->irq_lbl, np);
		if (err) {
			for (i--, vec--; i >= 0; i--) {
				np = adpt->qnapi[i];
				free_irq(adpt->msix_ent[vec].vector, np);
			}
			free_irq(adpt->msix_ent[0].vector, adpt);
			goto out;
		}
		vec++;
	}

out:
	return err;
}

static void alx_disable_msix(struct alx_adapter *adpt)
{
	if (adpt->msix_ent) {
		pci_disable_msix(adpt->pdev);
		kfree(adpt->msix_ent);
		adpt->msix_ent = NULL;
	}
	ALX_FLAG_CLEAR(adpt, USING_MSIX);
}

static void alx_disable_msi(struct alx_adapter *adpt)
{
	if (ALX_FLAG(adpt, USING_MSI)) {
		pci_disable_msi(adpt->pdev);
		ALX_FLAG_CLEAR(adpt, USING_MSI);
	}
}

static int txq_vec_mapping_shift[] = {
	0, ALX_MSI_MAP_TBL1_TXQ0_SHIFT,
	0, ALX_MSI_MAP_TBL1_TXQ1_SHIFT,
	1, ALX_MSI_MAP_TBL2_TXQ2_SHIFT,
	1, ALX_MSI_MAP_TBL2_TXQ3_SHIFT,
};
static int rxq_vec_mapping_shift[] = {
	0, ALX_MSI_MAP_TBL1_RXQ0_SHIFT,
	0, ALX_MSI_MAP_TBL1_RXQ1_SHIFT,
	0, ALX_MSI_MAP_TBL1_RXQ2_SHIFT,
	0, ALX_MSI_MAP_TBL1_RXQ3_SHIFT,
	1, ALX_MSI_MAP_TBL2_RXQ4_SHIFT,
	1, ALX_MSI_MAP_TBL2_RXQ5_SHIFT,
	1, ALX_MSI_MAP_TBL2_RXQ6_SHIFT,
	1, ALX_MSI_MAP_TBL2_RXQ7_SHIFT,
};
static void alx_config_vector_mapping(struct alx_adapter *adpt)
{
	struct alx_hw *hw = &adpt->hw;
	u32 tbl[2];
	int vect, idx, shft;
	int i;

	tbl[0] = tbl[1] = 0;

	if (ALX_FLAG(adpt, USING_MSIX)) {
		for (vect = 1, i = 0; i < adpt->nr_txq; i++, vect++) {
			idx = txq_vec_mapping_shift[i * 2];
			shft = txq_vec_mapping_shift[i * 2 + 1];
			tbl[idx] |= vect << shft;
		}
		for (vect = 1, i = 0; i < adpt->nr_rxq; i++, vect++) {
			idx = rxq_vec_mapping_shift[i * 2];
			shft = rxq_vec_mapping_shift[i * 2 + 1];
			tbl[idx] |= vect << shft;
		}
	}
	ALX_MEM_W32(hw, ALX_MSI_MAP_TBL1, tbl[0]);
	ALX_MEM_W32(hw, ALX_MSI_MAP_TBL2, tbl[1]);
	ALX_MEM_W32(hw, ALX_MSI_ID_MAP, 0);
}

void alx_disable_advanced_intr(struct alx_adapter *adpt)
{
	alx_disable_msix(adpt);
	alx_disable_msi(adpt);

	/* clear vector/intr-event mapping */
	alx_config_vector_mapping(adpt);
}

#endif

static void
alx_intr_enable(struct alx_softc *sc)
{
	struct alx_hw *hw;
#ifdef __notyet
	int i;
#endif

#if 0
	if (!atomic_dec_and_test(&adpt->irq_sem))
		return;
#endif

	hw = &sc->hw;

	/*
	 * XXX the linux code has the following line here-ish:
	 *
	 * ALX_MEM_W32(hw, ALX_ISR, (uint32_t)~ALX_ISR_DIS)
	 *
	 * with the comment 'clear old interrupts.'
	 */

	/* level-1 interrupt switch */
	ALX_MEM_W32(hw, ALX_ISR, 0);
	ALX_MEM_W32(hw, ALX_IMR, hw->imask);
	ALX_MEM_FLUSH(hw);

	if (!ALX_FLAG(sc, USING_MSIX))
		return;

#ifdef __notyet
	/* enable all individual MSIX IRQs */
	for (i = 0; i < adpt->nr_vec; i++)
		alx_mask_msix(hw, i, false);
#endif
}

static void
alx_intr_disable(struct alx_softc *sc)
{
	struct alx_hw *hw = &sc->hw;
	int i;

	//atomic_inc(&adpt->irq_sem);

	ALX_MEM_W32(hw, ALX_ISR, ALX_ISR_DIS);
	ALX_MEM_W32(hw, ALX_IMR, 0);
	ALX_MEM_FLUSH(hw);

	if (ALX_FLAG(sc, USING_MSIX))
		for (i = 0; i < sc->nr_vec; i++)
			alx_mask_msix(hw, i, true);
}

#if 0
static int alx_request_irq(struct alx_adapter *adpt)
{
	struct pci_dev *pdev = adpt->pdev;
	struct alx_hw *hw = &adpt->hw;
	int err;
	u32 msi_ctrl;

	msi_ctrl = FIELDX(ALX_MSI_RETRANS_TM, hw->imt >> 1);

	if (ALX_FLAG(adpt, USING_MSIX)) {
		ALX_MEM_W32(hw, ALX_MSI_RETRANS_TIMER, msi_ctrl);
		err = alx_request_msix(adpt);
		if (!err)
			goto out;
		/* fall back to MSI or legacy interrupt mode,
		 * re-alloc all resources
		 */
		alx_free_all_ring_resources(adpt);
		alx_disable_msix(adpt);
		adpt->nr_rxq = 1;
		adpt->nr_txq = 1;
		adpt->nr_napi = 1;
		adpt->nr_vec = 1;
		adpt->nr_hwrxq = 1;
		alx_configure_rss(hw, false);
		if (!pci_enable_msi(pdev))
			ALX_FLAG_SET(adpt, USING_MSI);

		err = alx_setup_all_ring_resources(adpt);
		if (err)
			goto out;
	}

	if (ALX_FLAG(adpt, USING_MSI)) {
		ALX_MEM_W32(hw, ALX_MSI_RETRANS_TIMER,
			    msi_ctrl | ALX_MSI_MASK_SEL_LINE);
		err = request_irq(pdev->irq, alx_intr_msi, 0,
				  adpt->netdev->name, adpt);
		if (!err)
			goto out;
		/* fall back to legacy interrupt */
		alx_disable_msi(adpt);
	}

	ALX_MEM_W32(hw, ALX_MSI_RETRANS_TIMER, 0);
	err = request_irq(pdev->irq, alx_intr_legacy, IRQF_SHARED,
			  adpt->netdev->name, adpt);

	if (err)
		netif_err(adpt, intr, adpt->netdev,
			  "request shared irq failed, err = %d\n",
			  err);

out:
	if (likely(!err)) {
		alx_config_vector_mapping(adpt);

		netif_info(adpt, drv, adpt->netdev,
			   "nr_rxq=%d, nr_txq=%d, nr_napi=%d, nr_vec=%d\n",
			   adpt->nr_rxq, adpt->nr_txq,
			   adpt->nr_napi, adpt->nr_vec);
		netif_info(adpt, drv, adpt->netdev,
			   "flags=%lX, Interrupt Mode: %s\n",
			   adpt->flags,
			   ALX_FLAG(adpt, USING_MSIX) ? "MSIX" :
			   ALX_FLAG(adpt, USING_MSI) ? "MSI" : "INTx");
	} else
		netdev_err(adpt->netdev,
			   "register IRQ fail %d\n",
			   err);

	return err;
}

static void alx_free_irq(struct alx_adapter *adpt)
{
	struct pci_dev *pdev = adpt->pdev;
	int i, vec;

	if (ALX_FLAG(adpt, USING_MSIX)) {
		free_irq(adpt->msix_ent[0].vector, adpt);
		vec = 1;
		for (i = 0; i < adpt->nr_napi; i++, vec++)
			free_irq(adpt->msix_ent[vec].vector, adpt->qnapi[i]);
	} else {
		free_irq(pdev->irq, adpt);
	}
	alx_disable_advanced_intr(adpt);
}
#endif

static int
alx_identify_hw(struct alx_softc *sc)
{
	device_t dev = sc->alx_dev;
	struct alx_hw *hw = &sc->hw;
	int rev;

	hw->device_id = pci_get_device(dev);
	hw->subdev_id = pci_get_subdevice(dev);
	hw->subven_id = pci_get_subvendor(dev);
	hw->revision = pci_get_revid(dev);
	rev = ALX_REVID(hw);

	switch (ALX_DID(hw)) {
	case ALX_DEV_ID_AR8161:
	case ALX_DEV_ID_AR8162:
	case ALX_DEV_ID_AR8171:
	case ALX_DEV_ID_AR8172:
		if (rev > ALX_REV_C0)
			break;
		ALX_CAP_SET(hw, L0S);
		ALX_CAP_SET(hw, L1);
		ALX_CAP_SET(hw, MTQ);
		ALX_CAP_SET(hw, RSS);
		ALX_CAP_SET(hw, MSIX);
		ALX_CAP_SET(hw, SWOI);
		hw->max_dma_chnl = rev >= ALX_REV_B0 ? 4 : 2;
		if (rev < ALX_REV_C0) {
			hw->ptrn_ofs = 0x600;
			hw->max_ptrns = 8;
		} else {
			hw->ptrn_ofs = 0x14000;
			hw->max_ptrns = 16;
		}
		break;
	default:
		return (EINVAL);
	}

	if (ALX_DID(hw) & 1)
		ALX_CAP_SET(hw, GIGA);

	return (0);
}

static const u8 def_rss_key[40] = {
	0xE2, 0x91, 0xD7, 0x3D, 0x18, 0x05, 0xEC, 0x6C,
	0x2A, 0x94, 0xB3, 0x0D, 0xA5, 0x4F, 0x2B, 0xEC,
	0xEA, 0x49, 0xAF, 0x7C, 0xE2, 0x14, 0xAD, 0x3D,
	0xB8, 0x55, 0xAA, 0xBE, 0x6A, 0x3E, 0x67, 0xEA,
	0x14, 0x36, 0x4D, 0x17, 0x3B, 0xED, 0x20, 0x0D,
};

#if 0
void alx_init_def_rss_idt(struct alx_adapter *adpt)
{
	struct alx_hw *hw = &adpt->hw;
	int i, x, y;
	u32 val;

	for (i = 0; i < hw->rss_idt_size; i++) {
		val = ethtool_rxfh_indir_default(i, adpt->nr_rxq);
		x = i >> 3;
		y = i * 4 & 0x1F;
		hw->rss_idt[x] &= ~(0xF << y);
		hw->rss_idt[x] |= (val & 0xF) << y;
	}
}

#endif

/* alx_init_adapter -
 *    initialize general software structure (struct alx_adapter).
 *    fields are inited based on PCI device information.
 */
static int
alx_init_sw(struct alx_softc *sc)
{
	device_t dev = sc->alx_dev;
	struct alx_hw *hw = &sc->hw;
	int i, err;

	err = alx_identify_hw(sc);
	if (err) {
		device_printf(dev, "unrecognized chip, aborting\n");
		return (err);
	}

	/* assign patch flag for specific platforms */
	alx_patch_assign(hw);

	memcpy(hw->rss_key, def_rss_key, sizeof(def_rss_key));
	hw->rss_idt_size = 128;
	hw->rss_hash_type = ALX_RSS_HASH_TYPE_ALL;
	hw->smb_timer = 400;
	hw->mtu = 1500; // XXX sc->alx_ifp->if_mtu;
	sc->rxbuf_size = ALX_RAW_MTU(hw->mtu) << 8;
	sc->tx_ringsz = 256;
	sc->rx_ringsz = 512;
	hw->sleep_ctrl = ALX_SLEEP_WOL_MAGIC | ALX_SLEEP_WOL_PHY;
	hw->imt = 200;
	hw->imask = ALX_ISR_MISC;
	hw->dma_chnl = hw->max_dma_chnl;
	hw->ith_tpd = sc->tx_ringsz / 3;
	hw->link_up = false;
	hw->link_duplex = 0;
	hw->link_speed = SPEED_0;
	hw->adv_cfg = ADVERTISED_Autoneg | ADVERTISED_10baseT_Half |
	    ADVERTISED_10baseT_Full | ADVERTISED_100baseT_Full |
	    ADVERTISED_100baseT_Half | ADVERTISED_1000baseT_Full;
	hw->flowctrl = ALX_FC_ANEG | ALX_FC_RX | ALX_FC_TX;
	hw->wrr_ctrl = ALX_WRR_PRI_RESTRICT_NONE;
	for (i = 0; i < ARRAY_SIZE(hw->wrr); i++)
		hw->wrr[i] = 4;

	hw->rx_ctrl = ALX_MAC_CTRL_WOLSPED_SWEN | ALX_MAC_CTRL_MHASH_ALG_HI5B |
	    ALX_MAC_CTRL_BRD_EN | ALX_MAC_CTRL_PCRCE | ALX_MAC_CTRL_CRCE |
	    ALX_MAC_CTRL_RXFC_EN | ALX_MAC_CTRL_TXFC_EN |
	    FIELDX(ALX_MAC_CTRL_PRMBLEN, 7);
	hw->is_fpga = false;

	//atomic_set(&adpt->irq_sem, 1);
	sc->irq_sem = 1;
	ALX_FLAG_SET(sc, HALT);

	return err;
}

#if 0
static void alx_set_vlan_mode(struct alx_hw *hw,
			      netdev_features_t features)
{
	if (features & NETIF_F_HW_VLAN_RX)
		hw->rx_ctrl |= ALX_MAC_CTRL_VLANSTRIP;
	else
		hw->rx_ctrl &= ~ALX_MAC_CTRL_VLANSTRIP;

	ALX_MEM_W32(hw, ALX_MAC_CTRL, hw->rx_ctrl);
}


static netdev_features_t alx_fix_features(struct net_device *netdev,
					  netdev_features_t features)
{
	/*
	 * Since there is no support for separate rx/tx vlan accel
	 * enable/disable make sure tx flag is always in same state as rx.
	 */
	if (features & NETIF_F_HW_VLAN_RX)
		features |= NETIF_F_HW_VLAN_TX;
	else
		features &= ~NETIF_F_HW_VLAN_TX;

	if (netdev->mtu > ALX_MAX_TSO_PKT_SIZE)
		features &= ~(NETIF_F_TSO | NETIF_F_TSO6);

	return features;
}


static int alx_set_features(struct net_device *netdev,
			    netdev_features_t features)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	netdev_features_t changed = netdev->features ^ features;

	if (!(changed & NETIF_F_HW_VLAN_RX))
		return 0;

	alx_set_vlan_mode(&adpt->hw, features);

	return 0;
}

/* alx_change_mtu - Change the Maximum Transfer Unit */
static int alx_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	int old_mtu   = netdev->mtu;
	int max_frame = new_mtu + ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN;

	if ((max_frame < ALX_MIN_FRAME_SIZE) ||
	    (max_frame > ALX_MAX_FRAME_SIZE)) {
		netif_err(adpt, hw, netdev,
			  "invalid MTU setting (%x)\n",
			  new_mtu);
		return -EINVAL;
	}
	/* set MTU */
	if (old_mtu != new_mtu) {
		netif_info(adpt, drv, adpt->netdev,
			   "changing MTU from %d to %d\n",
			   netdev->mtu, new_mtu);
		netdev->mtu = new_mtu;
		adpt->hw.mtu = new_mtu;
		adpt->rxbuf_size = new_mtu > ALX_DEF_RXBUF_SIZE ?
				   ALIGN(max_frame, 8) : ALX_DEF_RXBUF_SIZE;
		netdev_update_features(netdev);
		if (netif_running(netdev))
			alx_reinit(adpt, false);
	}

	return 0;
}

/* configure hardware everything except:
 *  1. interrupt vectors
 *  2. enable control for rx modules
 */
void alx_configure(struct alx_adapter *adpt)
{
	struct alx_hw *hw = &adpt->hw;

	alx_configure_basic(hw);
	alx_configure_rss(hw, adpt->nr_rxq > 1);
	__alx_set_rx_mode(adpt->netdev);
	alx_set_vlan_mode(hw, adpt->netdev->features);
}

static void alx_netif_stop(struct alx_adapter *adpt)
{
	int i;

	adpt->netdev->trans_start = jiffies;
	if (netif_carrier_ok(adpt->netdev)) {
		netif_carrier_off(adpt->netdev);
		netif_tx_disable(adpt->netdev);
		for (i = 0; i < adpt->nr_napi; i++)
			napi_disable(&adpt->qnapi[i]->napi);
	}
}

static void alx_netif_start(struct alx_adapter *adpt)
{
	int i;

	netif_tx_wake_all_queues(adpt->netdev);
	for (i = 0; i < adpt->nr_napi; i++)
		napi_enable(&adpt->qnapi[i]->napi);
	netif_carrier_on(adpt->netdev);
}

static bool alx_enable_msix(struct alx_adapter *adpt)
{
	int nr_txq, nr_rxq, vec_req;
	int i, err;

	nr_txq = min_t(int, num_online_cpus(), ALX_MAX_TX_QUEUES);
	nr_rxq = min_t(int, num_online_cpus(), ALX_MAX_RX_QUEUES);
	nr_rxq = rounddown_pow_of_two(nr_rxq);
	/* one more vector for PHY link change & timer & other events */
	vec_req = max_t(int, nr_txq, nr_rxq) + 1;

	if (vec_req  <= 2) {
		netif_info(adpt, intr, adpt->netdev,
			   "cpu core num is less, MSI-X isn't necessary\n");
		return false;
	}

	adpt->msix_ent = kcalloc(vec_req,
				 sizeof(struct msix_entry),
				 GFP_KERNEL);
	if (!adpt->msix_ent) {
		netif_warn(adpt, intr, adpt->netdev,
			   "can't alloc msix entries\n");
		return false;
	}
	for (i = 0; i < vec_req; i++)
		adpt->msix_ent[i].entry = i;

	err = pci_enable_msix(adpt->pdev, adpt->msix_ent, vec_req);
	if (err) {
		kfree(adpt->msix_ent);
		adpt->msix_ent = NULL;
		netif_warn(adpt, intr, adpt->netdev,
			   "can't enable MSI-X interrupt\n");
		return false;
	}

	adpt->nr_txq = nr_txq;
	adpt->nr_rxq = nr_rxq;
	adpt->nr_vec = vec_req;
	adpt->nr_napi = vec_req - 1;
	adpt->nr_hwrxq = ALX_CAP(&adpt->hw, MRQ) ? adpt->nr_rxq : 1;

	return true;
}

void alx_init_intr(struct alx_adapter *adpt)
{
	struct alx_hw *hw = &adpt->hw;

	if ((ALX_CAP(hw, MTQ) || ALX_CAP(hw, RSS)) && ALX_CAP(hw, MSIX)) {
		if (alx_enable_msix(adpt))
			ALX_FLAG_SET(adpt, USING_MSIX);
	}
	if (!ALX_FLAG(adpt, USING_MSIX)) {
		adpt->nr_txq = 1;
		adpt->nr_rxq = 1;
		adpt->nr_napi = 1;
		adpt->nr_vec = 1;
		adpt->nr_hwrxq = 1;

		if (!pci_enable_msi(adpt->pdev))
			ALX_FLAG_SET(adpt, USING_MSI);
	}
}

static void alx_halt(struct alx_adapter *adpt, bool in_task)
{
	struct alx_hw *hw = &adpt->hw;

	ALX_FLAG_SET(adpt, HALT);
	if (!in_task)
		alx_cancel_work(adpt);

	alx_netif_stop(adpt);
	hw->link_up = false;
	hw->link_speed = SPEED_0;

	alx_reset_mac(hw);

	/* disable l0s/l1 */
	alx_enable_aspm(hw, false, false);
	alx_irq_disable(adpt);
	alx_free_all_rings_buf(adpt);
}

static void alx_activate(struct alx_adapter *adpt)
{
	/* hardware setting lost, restore it */
	alx_reinit_rings(adpt);
	alx_configure(adpt);

	ALX_FLAG_CLEAR(adpt, HALT);
	/* clear old interrupts */
	ALX_MEM_W32(&adpt->hw, ALX_ISR, (u32)~ALX_ISR_DIS);

	alx_irq_enable(adpt);

	ALX_FLAG_SET(adpt, TASK_CHK_LINK);
	alx_schedule_work(adpt);
}

static void alx_show_speed(struct alx_adapter *adpt, u16 speed)
{
	netif_info(adpt, link, adpt->netdev,
		   "NIC Link Up: %s\n",
		   speed_desc(speed));
}

static int alx_reinit_rings(struct alx_adapter *adpt)
{
	int i, err = 0;

	alx_free_all_rings_buf(adpt);

	/* set rings' header to HW register */
	alx_init_ring_ptrs(adpt);

	/* alloc hw-rxing buf */
	for (i = 0; i < adpt->nr_hwrxq; i++) {
		int count;

		count = alx_alloc_rxring_buf(adpt, adpt->qnapi[i]->rxq);
		if (unlikely(!count)) {
			err = -ENOMEM;
			break;
		}
	}

	return err;
}



static void alx_check_link(struct alx_adapter *adpt)
{
	struct alx_hw *hw = &adpt->hw;
	u16 speed, old_speed;
	bool link_up, old_link_up;
	int err;

	if (ALX_FLAG(adpt, HALT))
		return;

	/* clear PHY internal interrupt status,
	 * otherwise the Main interrupt status will be asserted
	 * for ever.
	 */
	alx_clear_phy_intr(hw);

	err = alx_get_phy_link(hw, &link_up, &speed);
	if (err)
		goto out;

	/* open interrutp mask */
	hw->imask |= ALX_ISR_PHY;
	ALX_MEM_W32(hw, ALX_IMR, hw->imask);

	if (!link_up && !hw->link_up)
		goto out;

	old_speed = hw->link_speed + hw->link_duplex;
	old_link_up = hw->link_up;

	if (link_up) {
		/* same speed ? */
		if (old_link_up && old_speed == speed)
			goto out;

		alx_show_speed(adpt, speed);
		hw->link_duplex = speed % 10;
		hw->link_speed = speed - hw->link_duplex;
		hw->link_up = true;
		alx_post_phy_link(hw, hw->link_speed, ALX_CAP(hw, AZ));
		alx_enable_aspm(hw, ALX_CAP(hw, L0S), ALX_CAP(hw, L1));
		alx_start_mac(hw);

		/* link kept, just speed changed */
		if (old_link_up)
			goto out;
		/* link changed from 'down' to 'up' */
		alx_netif_start(adpt);
		goto out;
	}

	/* link changed from 'up' to 'down' */
	alx_netif_stop(adpt);
	hw->link_up = false;
	hw->link_speed = SPEED_0;
	netif_info(adpt, link, adpt->netdev, "NIC Link Down\n");
	err = alx_reset_mac(hw);
	if (err) {
		netif_err(adpt, hw, adpt->netdev,
			  "linkdown:reset_mac fail %d\n", err);
		err = -EIO;
		goto out;
	}
	alx_irq_disable(adpt);

	/* reset-mac cause all settings on HW lost,
	 * following steps restore all of them and
	 * refresh whole RX/TX rings
	 */
	err = alx_reinit_rings(adpt);
	if (err)
		goto out;
	alx_configure(adpt);
	alx_enable_aspm(hw, false, ALX_CAP(hw, L1));
	alx_post_phy_link(hw, SPEED_0, ALX_CAP(hw, AZ));
	alx_irq_enable(adpt);

out:

	if (err) {
		ALX_FLAG_SET(adpt, TASK_RESET);
		alx_schedule_work(adpt);
	}
}

static int __alx_shutdown(struct pci_dev *pdev, bool *wol_en)
{
	struct alx_adapter *adpt = pci_get_drvdata(pdev);
	struct net_device *netdev = adpt->netdev;
	struct alx_hw *hw = &adpt->hw;
	int err;
	u16 speed;

	netif_device_detach(netdev);
	if (netif_running(netdev))
		__alx_stop(adpt);

#ifdef CONFIG_PM_SLEEP
	err = pci_save_state(pdev);
	if (err)
		goto out;
#endif

	err = alx_select_powersaving_speed(hw, &speed);
	if (!err)
		err = alx_clear_phy_intr(hw);
	if (!err)
		err = alx_pre_suspend(hw, speed);
	if (!err)
		err = alx_config_wol(hw);
	if (err)
		goto out;

	*wol_en = false;
	if (hw->sleep_ctrl & ALX_SLEEP_ACTIVE) {
		netif_info(adpt, wol, netdev,
			   "wol: ctrl=%X, speed=%X\n",
			   hw->sleep_ctrl, speed);

		device_set_wakeup_enable(&pdev->dev, true);
		*wol_en = true;
	}

	pci_disable_device(pdev);

out:
	if (unlikely(err)) {
		netif_info(adpt, hw, netdev,
			   "shutown err(%x)\n",
			   err);
		err = -EIO;
	}

	return err;
}

static void alx_shutdown(struct pci_dev *pdev)
{
	int err;
	bool wol_en;

	err = __alx_shutdown(pdev, &wol_en);
	if (likely(!err)) {
		pci_wake_from_d3(pdev, wol_en);
		pci_set_power_state(pdev, PCI_D3hot);
	} else {
		dev_err(&pdev->dev, "shutdown fail %d\n", err);
	}
}

#ifdef CONFIG_PM_SLEEP
static int alx_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	int err;
	bool wol_en;

	err = __alx_shutdown(pdev, &wol_en);
	if (unlikely(err)) {
		dev_err(&pdev->dev, "shutdown fail in suspend %d\n", err);
		err = -EIO;
		goto out;
	}
	if (wol_en) {
		pci_prepare_to_sleep(pdev);
	} else {
		pci_wake_from_d3(pdev, false);
		pci_set_power_state(pdev, PCI_D3hot);
	}

out:
	return err;
}

static int alx_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct alx_adapter *adpt = pci_get_drvdata(pdev);
	struct net_device *netdev = adpt->netdev;
	struct alx_hw *hw = &adpt->hw;
	int err;

	if (!netif_running(netdev))
		return 0;

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	pci_save_state(pdev);

	pci_enable_wake(pdev, PCI_D3hot, 0);
	pci_enable_wake(pdev, PCI_D3cold, 0);

	hw->link_up = false;
	hw->link_speed = SPEED_0;
	hw->imask = ALX_ISR_MISC;

	alx_reset_pcie(hw);
	alx_reset_phy(hw, !hw->hib_patch);
	err = alx_reset_mac(hw);
	if (err) {
		netif_err(adpt, hw, adpt->netdev,
			  "resume:reset_mac fail %d\n",
			  err);
		return -EIO;
	}
	err = alx_setup_speed_duplex(hw, hw->adv_cfg, hw->flowctrl);
	if (err) {
		netif_err(adpt, hw, adpt->netdev,
			  "resume:setup_speed_duplex fail %d\n",
			  err);
		return -EIO;
	}

	if (netif_running(netdev)) {
		err = __alx_open(adpt, true);
		if (err)
			return err;
	}

	netif_device_attach(netdev);

	return err;
}
#endif



/* alx_update_hw_stats - Update the board statistics counters. */
static void alx_update_hw_stats(struct alx_adapter *adpt)
{
	if (ALX_FLAG(adpt, HALT) || ALX_FLAG(adpt, RESETING))
		return;

	__alx_update_hw_stats(&adpt->hw);
}

/* alx_get_stats - Get System Network Statistics
 *
 * Returns the address of the device statistics structure.
 * The statistics are actually updated from the timer callback.
 */
static struct net_device_stats *alx_get_stats(struct net_device *netdev)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	struct net_device_stats *net_stats = &netdev->stats;
	struct alx_hw_stats *hw_stats = &adpt->hw.stats;

	spin_lock(&adpt->smb_lock);

	alx_update_hw_stats(adpt);

	net_stats->tx_packets = hw_stats->tx_ok;
	net_stats->tx_bytes   = hw_stats->tx_byte_cnt;
	net_stats->rx_packets = hw_stats->rx_ok;
	net_stats->rx_bytes   = hw_stats->rx_byte_cnt;
	net_stats->multicast  = hw_stats->rx_mcast;
	net_stats->collisions = hw_stats->tx_single_col +
				hw_stats->tx_multi_col * 2 +
				hw_stats->tx_late_col + hw_stats->tx_abort_col;

	net_stats->rx_errors  = hw_stats->rx_frag + hw_stats->rx_fcs_err +
				hw_stats->rx_len_err + hw_stats->rx_ov_sz +
				hw_stats->rx_ov_rrd + hw_stats->rx_align_err;

	net_stats->rx_fifo_errors   = hw_stats->rx_ov_rxf;
	net_stats->rx_length_errors = hw_stats->rx_len_err;
	net_stats->rx_crc_errors    = hw_stats->rx_fcs_err;
	net_stats->rx_frame_errors  = hw_stats->rx_align_err;
	net_stats->rx_over_errors   = hw_stats->rx_ov_rrd + hw_stats->rx_ov_rxf;

	net_stats->rx_missed_errors = hw_stats->rx_ov_rrd + hw_stats->rx_ov_rxf;

	net_stats->tx_errors = hw_stats->tx_late_col + hw_stats->tx_abort_col +
			       hw_stats->tx_underrun + hw_stats->tx_trunc;

	net_stats->tx_aborted_errors = hw_stats->tx_abort_col;
	net_stats->tx_fifo_errors    = hw_stats->tx_underrun;
	net_stats->tx_window_errors  = hw_stats->tx_late_col;

	spin_unlock(&adpt->smb_lock);

	return net_stats;
}

static void alx_update_stats(struct alx_adapter *adpt)
{
	spin_lock(&adpt->smb_lock);
	alx_update_hw_stats(adpt);
	spin_unlock(&adpt->smb_lock);
}

void alx_reinit(struct alx_adapter *adpt, bool in_task)
{
	WARN_ON(in_interrupt());

	while (test_and_set_bit(ALX_FLAG_RESETING, &adpt->flags))
		msleep(20);

	if (ALX_FLAG(adpt, HALT))
		return;

	alx_halt(adpt, in_task);
	alx_activate(adpt);

	ALX_FLAG_CLEAR(adpt, RESETING);
}

/* alx_task - manages and runs subtasks */
static void alx_task(struct work_struct *work)
{
	struct alx_adapter *adpt = container_of(work, struct alx_adapter, task);

	/* don't support reentrance */
	while (test_and_set_bit(ALX_FLAG_TASK_PENDING, &adpt->flags))
		msleep(20);

	if (ALX_FLAG(adpt, HALT))
		goto out;

	if (test_and_clear_bit(ALX_FLAG_TASK_RESET, &adpt->flags)) {
		netif_info(adpt, hw, adpt->netdev,
			   "task:alx_reinit\n");
		alx_reinit(adpt, true);
	}

	if (test_and_clear_bit(ALX_FLAG_TASK_UPDATE_SMB, &adpt->flags))
		alx_update_stats(adpt);

	if (test_and_clear_bit(ALX_FLAG_TASK_CHK_LINK, &adpt->flags))
		alx_check_link(adpt);

out:
	ALX_FLAG_CLEAR(adpt, TASK_PENDING);
}


static irqreturn_t alx_msix_ring(int irq, void *data)
{
	struct alx_napi *np = data;
	struct alx_adapter *adpt = np->adpt;
	struct alx_hw *hw = &adpt->hw;

	/* mask interrupt to ACK chip */
	alx_mask_msix(hw, np->vec_idx, true);
	/* clear interrutp status */
	ALX_MEM_W32(hw, ALX_ISR, np->vec_mask);

	if (!ALX_FLAG(adpt, HALT))
		napi_schedule(&np->napi);

	return IRQ_HANDLED;
}

static inline bool alx_handle_intr_misc(struct alx_adapter *adpt, u32 intr)
{
	struct alx_hw *hw = &adpt->hw;

	if (unlikely(intr & ALX_ISR_FATAL)) {
		netif_info(adpt, hw, adpt->netdev,
			   "intr-fatal:%08X\n", intr);
		ALX_FLAG_SET(adpt, TASK_RESET);
		alx_schedule_work(adpt);
		return true;
	}

	if (intr & ALX_ISR_ALERT)
		netdev_warn(adpt->netdev, "interrutp alert :%x\n", intr);

	if (intr & ALX_ISR_SMB) {
		ALX_FLAG_SET(adpt, TASK_UPDATE_SMB);
		alx_schedule_work(adpt);
	}

	if (intr & ALX_ISR_PHY) {
		/* suppress PHY interrupt, because the source
		 * is from PHY internal. only the internal status
		 * is cleared, the interrupt status could be cleared.
		 */
		hw->imask &= ~ALX_ISR_PHY;
		ALX_MEM_W32(hw, ALX_IMR, hw->imask);
		ALX_FLAG_SET(adpt, TASK_CHK_LINK);
		alx_schedule_work(adpt);
	}

	return false;
}

static irqreturn_t alx_intr_msix_misc(int irq, void *data)
{
	struct alx_adapter *adpt = data;
	struct alx_hw *hw = &adpt->hw;
	u32 intr;

	/* mask interrupt to ACK chip */
	alx_mask_msix(hw, 0, true);

	/* read interrupt status */
	ALX_MEM_R32(hw, ALX_ISR, &intr);
	intr &= (hw->imask & ~ALX_ISR_ALL_QUEUES);

	if (alx_handle_intr_misc(adpt, intr))
		return IRQ_HANDLED;

	/* clear interrupt status */
	ALX_MEM_W32(hw, ALX_ISR, intr);

	/* enable interrupt again */
	if (!ALX_FLAG(adpt, HALT))
		alx_mask_msix(hw, 0, false);

	return IRQ_HANDLED;
}

static inline irqreturn_t alx_intr_1(struct alx_adapter *adpt, u32 intr)
{
	struct alx_hw *hw = &adpt->hw;

	/* ACK interrupt */
	netif_info(adpt, intr, adpt->netdev,
		   "ACK interrupt: 0x%lx\n",
		   intr | ALX_ISR_DIS);
	ALX_MEM_W32(hw, ALX_ISR, intr | ALX_ISR_DIS);
	intr &= hw->imask;

	if (alx_handle_intr_misc(adpt, intr))
		return IRQ_HANDLED;

	if (intr & (ALX_ISR_TX_Q0 | ALX_ISR_RX_Q0)) {
		napi_schedule(&adpt->qnapi[0]->napi);
		/* mask rx/tx interrupt, enable them when napi complete */
		hw->imask &= ~ALX_ISR_ALL_QUEUES;
		ALX_MEM_W32(hw, ALX_IMR, hw->imask);
	}

	ALX_MEM_W32(hw, ALX_ISR, 0);

	return IRQ_HANDLED;
}


static irqreturn_t alx_intr_msi(int irq, void *data)
{
	struct alx_adapter *adpt = data;
	u32 intr;

	/* read interrupt status */
	ALX_MEM_R32(&adpt->hw, ALX_ISR, &intr);

	return alx_intr_1(adpt, intr);
}

static irqreturn_t alx_intr_legacy(int irq, void *data)
{
	struct alx_adapter *adpt = data;
	struct alx_hw *hw = &adpt->hw;
	u32 intr;

	/* read interrupt status */
	ALX_MEM_R32(hw, ALX_ISR, &intr);
	if (intr & ALX_ISR_DIS || 0 == (intr & hw->imask)) {
		u32 mask;

		ALX_MEM_R32(hw, ALX_IMR, &mask);
		netif_info(adpt, intr, adpt->netdev,
			   "seems a wild interrupt, intr=%X, imask=%X, %X\n",
			   intr, hw->imask, mask);

		return IRQ_NONE;
	}

	return alx_intr_1(adpt, intr);
}


static int alx_poll(struct napi_struct *napi, int budget)
{
	struct alx_napi *np = container_of(napi, struct alx_napi, napi);
	struct alx_adapter *adpt = np->adpt;
	bool complete = true;

	netif_info(adpt, intr, adpt->netdev,
		   "alx_poll, budget(%d)\n",
		   budget);

	if (np->txq)
		complete = alx_clean_tx_irq(np->txq);
	if (np->rxq)
		complete &= alx_clean_rx_irq(np->rxq, budget);

	if (!complete)
		return budget;

	/* rx-packet finished, exit the polling mode */
	napi_complete(&np->napi);

	/* enable interrupt */
	if (!ALX_FLAG(adpt, HALT)) {
		struct alx_hw *hw = &adpt->hw;

		if (ALX_FLAG(adpt, USING_MSIX))
			alx_mask_msix(hw, np->vec_idx, false);
		else {
			/* TODO: need irq spinlock for imask ?? */
			hw->imask |= ALX_ISR_TX_Q0 | ALX_ISR_RX_Q0;
			ALX_MEM_W32(hw, ALX_IMR, hw->imask);
		}
		ALX_MEM_FLUSH(hw);
	}

	return 0;
}

static inline struct alx_tx_queue *alx_tx_queue_mapping(
			struct alx_adapter *adpt,
			struct sk_buff *skb)
{
	int index = skb_get_queue_mapping(skb);

	if (index >= adpt->nr_txq)
		index = index % adpt->nr_txq;

	return adpt->qnapi[index]->txq;
}

static inline int alx_tpd_req(struct sk_buff *skb)
{
	int num;

	num = skb_shinfo(skb)->nr_frags + 1;
	if (skb_is_gso(skb) && skb_shinfo(skb)->gso_type & SKB_GSO_TCPV6)
		num++;

	return num;
}

/* get custom checksum offload params
 * return val:
 *     neg-val: drop this skb
 *     0: no custom checksum offload
 *     pos-val: have custom cksum offload
 */
static int alx_tx_csum(struct sk_buff *skb, struct tpd_desc *first)
{
	u8 cso, css;

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return 0;

	cso = skb_checksum_start_offset(skb);
	if (cso & 0x1)
		return -1;

	css = cso + skb->csum_offset;
	first->word1 |= FIELDX(TPD_CXSUMSTART, cso >> 1);
	first->word1 |= FIELDX(TPD_CXSUMOFFSET, css >> 1);
	first->word1 |= 1 << TPD_CXSUM_EN_SHIFT;

	return 1;
}

static int alx_tso(struct sk_buff *skb, struct tpd_desc *first)
{
	int hdr_len;
	int err;

	if (skb->ip_summed != CHECKSUM_PARTIAL ||
	    !skb_is_gso(skb))
		return 0;

	if (skb_header_cloned(skb)) {
		err = pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
		if (unlikely(err))
			return err;
	}

	if (skb->protocol == htons(ETH_P_IP)) {
		struct iphdr *iph;

		iph = ip_hdr(skb);
		hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
		iph->check = 0;
		tcp_hdr(skb)->check = ~csum_tcpudp_magic(
						iph->saddr,
						iph->daddr,
						0, IPPROTO_TCP, 0);
		first->word1 |= 1 << TPD_IPV4_SHIFT;
		first->word1 |= FIELDX(TPD_L4HDROFFSET,
				       skb_transport_offset(skb));
		if (unlikely(skb->len == hdr_len)) {
			/* no tcp payload */
			first->word1 |= 1 << TPD_IP_XSUM_SHIFT;
			first->word1 |= 1 << TPD_TCP_XSUM_SHIFT;
			return 0;
		}
		first->word1 |= 1 << TPD_LSO_EN_SHIFT;
		first->word1 |= FIELDX(TPD_MSS, skb_shinfo(skb)->gso_size);
	} else if (skb_is_gso_v6(skb)) {
		struct ipv6hdr *ip6h;

		ip6h = ipv6_hdr(skb);
		hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
		ip6h->payload_len = 0;
		tcp_hdr(skb)->check = ~csum_ipv6_magic(
						&ip6h->saddr,
						&ip6h->daddr,
						0, IPPROTO_TCP, 0);
		first->word1 |= FIELDX(TPD_L4HDROFFSET,
				       skb_transport_offset(skb));
		if (unlikely(skb->len == hdr_len)) {
			/* no tcp payload */
			ip6h->payload_len = skb->len -
				((unsigned char *)ip6h - skb->data) -
				sizeof(struct ipv6hdr);
			first->word1 |= 1 << TPD_IP_XSUM_SHIFT;
			first->word1 |= 1 << TPD_TCP_XSUM_SHIFT;
			return 0;
		}
		/* for LSOv2, the 1st TPD just provides packet length */
		first->adrl.l.pkt_len = skb->len;
		first->word1 |= 1 << TPD_LSO_EN_SHIFT;
		first->word1 |= 1 << TPD_LSO_V2_SHIFT;
		first->word1 |= FIELDX(TPD_MSS, skb_shinfo(skb)->gso_size);
	}

	return 1;
}

static int alx_tx_map(struct alx_tx_queue *txq, struct sk_buff *skb)
{
	struct tpd_desc *tpd, *first_tpd;
	struct alx_buffer *buf, *first_buf;
	dma_addr_t dma;
	u16 producer, maplen, f;

	producer = txq->pidx;

	first_tpd = txq->tpd_hdr + producer;
	first_buf = txq->bf_info + producer;
	tpd = first_tpd;
	buf = first_buf;
	if (tpd->word1 & (1 << TPD_LSO_V2_SHIFT)) {
		if (++producer == txq->count)
			producer = 0;
		tpd = txq->tpd_hdr + producer;
		buf = txq->bf_info + producer;
		tpd->word0 = first_tpd->word0;
		tpd->word1 = first_tpd->word1;
	}
	maplen = skb_headlen(skb);
	dma = dma_map_single(txq->dev, skb->data, maplen, DMA_TO_DEVICE);
	if (dma_mapping_error(txq->dev, dma))
		goto err_dma;

	dma_unmap_len_set(buf, size, maplen);
	dma_unmap_addr_set(buf, dma, dma);

	tpd->adrl.addr = cpu_to_le64(dma);
	FIELD_SET32(tpd->word0, TPD_BUFLEN, maplen);

	for (f = 0; f < skb_shinfo(skb)->nr_frags; f++) {
		struct skb_frag_struct *frag;

		frag = &skb_shinfo(skb)->frags[f];
		if (++producer == txq->count)
			producer = 0;
		tpd = txq->tpd_hdr + producer;
		buf = txq->bf_info + producer;
		tpd->word0 = first_tpd->word0;
		tpd->word1 = first_tpd->word1;
		maplen = skb_frag_size(frag);
		dma = skb_frag_dma_map(txq->dev, frag, 0,
				       maplen, DMA_TO_DEVICE);
		if (dma_mapping_error(txq->dev, dma))
			goto err_dma;
		dma_unmap_len_set(buf, size, maplen);
		dma_unmap_addr_set(buf, dma, dma);

		tpd->adrl.addr = cpu_to_le64(dma);
		FIELD_SET32(tpd->word0, TPD_BUFLEN, maplen);
	}
	/* last TPD */
	tpd->word1 |= 1 << TPD_EOP_SHIFT;

	if (++producer == txq->count)
		producer = 0;

	first_buf->flags |= ALX_BUF_TX_FIRSTFRAG;
	buf->skb = skb;
	txq->pidx = producer;

	return 0;

err_dma:

	for (f = txq->pidx; f != producer;) {
		alx_txbuf_unmap_and_free(txq, f);
		if (++f == txq->count)
			f = 0;
	}
	return -1;
}

static netdev_tx_t alx_start_xmit_ring(struct alx_tx_queue *txq,
				       struct sk_buff *skb)
{
	struct alx_adapter *adpt;
	struct netdev_queue *netque;
	struct tpd_desc *first;
	int budget, tpdreq;
	int do_tso;

	adpt = netdev_priv(txq->netdev);
	netque = netdev_get_tx_queue(txq->netdev, skb_get_queue_mapping(skb));

	tpdreq = alx_tpd_req(skb);
	budget = alx_tpd_avail(txq);

	if (unlikely(budget < tpdreq)) {
		if (!netif_tx_queue_stopped(netque)) {
			netif_tx_stop_queue(netque);

			/* TX reclaim might have plenty of free TPD
			 * but see tx_queue is active (because its
			 * judement doesn't acquire tx-spin-lock,
			 * this situation cause the TX-queue stop and
			 * never be wakeup.
			 * try one more time
			 */
			budget = alx_tpd_avail(txq);
			if (budget >= tpdreq) {
				netif_tx_wake_queue(netque);
				goto tx_conti;
			}
			netif_err(adpt, tx_err, adpt->netdev,
				  "TPD Ring is full when queue awake!\n");
		}
		return NETDEV_TX_BUSY;
	}

tx_conti:

	first = txq->tpd_hdr + txq->pidx;
	memset(first, 0, sizeof(struct tpd_desc));

	/* NOTE, chip only supports single-VLAN insertion (81-00-TAG) */
	if (vlan_tx_tag_present(skb)) {
		first->word1 |= 1 << TPD_INS_VLTAG_SHIFT;
		first->word0 |= FIELDX(TPD_VLTAG, htons(vlan_tx_tag_get(skb)));
	}
	if (skb->protocol == htons(ETH_P_8021Q))
		first->word1 |= 1 << TPD_VLTAGGED_SHIFT;
	if (skb_network_offset(skb) != ETH_HLEN)
		first->word1 |= 1 << TPD_ETHTYPE_SHIFT;

	do_tso = alx_tso(skb, first);
	if (do_tso < 0)
		goto drop;
	else if (!do_tso && alx_tx_csum(skb, first) < 0)
		goto drop;

	if (alx_tx_map(txq, skb) < 0)
		goto drop;

	netdev_tx_sent_queue(netque, skb->len);

	/* refresh produce idx on HW */
	wmb();
	ALX_MEM_W16(&adpt->hw, txq->p_reg, txq->pidx);

	netif_info(adpt, tx_done, adpt->netdev,
		   "TX[Preg:%X]: producer = 0x%x, consumer = 0x%x\n",
		   txq->p_reg, txq->pidx, atomic_read(&txq->cidx));

	return NETDEV_TX_OK;

drop:
	netif_info(adpt, tx_done, adpt->netdev,
		   "tx-skb(%d) dropped\n",
		   skb->len);
	memset(first, 0, sizeof(struct tpd_desc));
	dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}

static netdev_tx_t alx_start_xmit(struct sk_buff *skb,
				  struct net_device *netdev)
{
	struct alx_adapter *adpt = netdev_priv(netdev);

	if (ALX_FLAG(adpt, HALT)) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (skb->len <= 0) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	return alx_start_xmit_ring(alx_tx_queue_mapping(adpt, skb), skb);
}


static void alx_dump_state(struct alx_adapter *adpt)
{
	struct alx_hw *hw = &adpt->hw;
	struct alx_tx_queue *txq;
	struct tpd_desc *tpd;
	u16 begin, end;
	int i;

	for (i = 0; i < adpt->nr_txq; i++) {

		txq = adpt->qnapi[i]->txq;
		begin = txq->pidx >=  8 ? (txq->pidx - 8) :
				(txq->count + txq->pidx - 8);
		end = txq->pidx + 4;
		if (end >= txq->count)
			end -= txq->count;

		netif_err(adpt, tx_err, adpt->netdev,
			  "-----------------TPD-ring(%d)------------------\n",
			  i);

		while (begin != end) {
			tpd = txq->tpd_hdr + begin;
			netif_err(adpt, tx_err, adpt->netdev,
				  "%X: W0=%08X, W1=%08X, W2=%X\n",
				  begin, tpd->word0, tpd->word1,
				  tpd->adrl.l.pkt_len);
			if (++begin >= txq->count)
				begin = 0;
		}
	}

	netif_err(adpt, tx_err, adpt->netdev,
		  "---------------dump registers-----------------\n");
	end = 0x1800;
	for (begin = 0x1400; begin < end; begin += 16) {
		u32 v1, v2, v3, v4;

		ALX_MEM_R32(hw, begin, &v1);
		ALX_MEM_R32(hw, begin+4, &v2);
		ALX_MEM_R32(hw, begin+8, &v3);
		ALX_MEM_R32(hw, begin+12, &v4);
		netif_err(adpt, tx_err, adpt->netdev,
			  "%04X: %08X,%08X,%08X,%08X\n",
			  begin, v1, v2, v3, v4);
	}
}

static void alx_tx_timeout(struct net_device *dev)
{
	struct alx_adapter *adpt = netdev_priv(dev);

	alx_dump_state(adpt);

	ALX_FLAG_SET(adpt, TASK_RESET);
	alx_schedule_work(adpt);
}

static int alx_mdio_read(struct net_device *netdev,
			 int prtad, int devad, u16 addr)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	struct alx_hw *hw = &adpt->hw;
	u16 val;
	int err;

	netif_dbg(adpt, hw, netdev,
		  "alx_mdio_read, prtad=%d, devad=%d, addr=%X\n",
		  prtad, devad, addr);

	if (prtad != hw->mdio.prtad)
		return -EINVAL;

	if (devad != MDIO_DEVAD_NONE)
		err = alx_read_phy_ext(hw, devad, addr, &val);
	else
		err = alx_read_phy_reg(hw, addr, &val);

	return err ? -EIO : val;
}

static int alx_mdio_write(struct net_device *netdev,
			  int prtad, int devad, u16 addr, u16 val)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	struct alx_hw *hw = &adpt->hw;
	int err;

	netif_dbg(adpt, hw, netdev,
		  "alx_mdio_write: prtad=%d, devad=%d, addr=%X, val=%X\n",
		  prtad, devad, addr, val);

	if (prtad != hw->mdio.prtad)
		return -EINVAL;

	if (devad != MDIO_DEVAD_NONE)
		err = alx_write_phy_ext(hw, devad, addr, val);
	else
		err = alx_write_phy_reg(hw, addr, val);

	return err ? -EIO : 0;
}

static int alx_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	struct alx_adapter *adpt = netdev_priv(netdev);

	if (!netif_running(netdev))
		return -EAGAIN;

	return mdio_mii_ioctl(&adpt->hw.mdio, if_mii(ifr), cmd);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void alx_poll_controller(struct net_device *netdev)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	int i;

	if (ALX_FLAG(adpt, HALT))
		return;

	if (ALX_FLAG(adpt, USING_MSIX)) {
		alx_intr_msix_misc(0, adpt);
		for (i = 0; i < adpt->nr_napi; i++)
			alx_msix_ring(0, adpt->qnapi[i]);
	} else if (ALX_FLAG(adpt, USING_MSI))
		alx_intr_msi(0, adpt);
	else
		alx_intr_legacy(0, adpt);

}
#endif

#endif

static void
alx_init_tx_ring(struct alx_softc *sc)
{
	struct alx_hw *hw;

	ALX_LOCK_ASSERT(sc);

	hw = &sc->hw;

	sc->alx_tx_queue.pidx = 0;
	sc->alx_tx_queue.cidx = 0;
	sc->alx_tx_queue.count = 0;

	/* XXX multiple queues */
	ALX_MEM_W32(hw, ALX_TPD_RING_SZ, sc->tx_ringsz);
	ALX_MEM_W32(hw, ALX_TPD_PRI0_ADDR_LO, sc->alx_tx_queue.tpd_dma);
	ALX_MEM_W32(hw, ALX_TX_BASE_ADDR_HI, sc->alx_tx_queue.tpd_dma >> 32);
}

static void
alx_init(void *arg)
{
	struct alx_softc *sc;

	sc = arg;
	ALX_LOCK(sc);
	alx_init_locked(sc);
	ALX_UNLOCK(sc);
}

static void
alx_init_locked(struct alx_softc *sc)
{
	struct ifnet *ifp;
	struct alx_hw *hw;

	ALX_LOCK_ASSERT(sc);

	ifp = sc->alx_ifp;
	hw = &sc->hw;

	alx_stop(sc);

	/* Reset to a known good state. */
	alx_reset(sc);

	memcpy(hw->mac_addr, IF_LLADDR(ifp), ETHER_ADDR_LEN);
	alx_set_macaddr(hw, hw->mac_addr);

	alx_init_tx_ring(sc);
	// alx_init_rx_ring(sc);

	/* Load the DMA pointers. */
	ALX_MEM_W32(hw, ALX_SRAM9, ALX_SRAM_LOAD_PTR);

	alx_configure_basic(hw);
	alx_configure_rss(hw, 0 /* XXX */);
	/*
	 * XXX configure some VLAN rx strip thingy and some promiscuous mode
	 * stuff and some multicast stuff.
	 */

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	alx_intr_enable(sc);
}

static int
alx_encap(struct alx_softc *sc, struct mbuf **m_head)
{
	struct mbuf *m;
	struct tpd_desc *td;
	bus_dma_segment_t seg;
	int first, error, nsegs, i;

	ALX_LOCK_ASSERT(sc);

	M_ASSERTPKTHDR(*m_head);

	/* XXX figure out how segments the DMA engine can support. */
retry:
	error = bus_dmamap_load_mbuf_sg(sc->alx_tx_tag, sc->alx_tx_dmamap,
	    *m_head, &seg, &nsegs, 0);
	if (error == EFBIG) {
		m = m_collapse(*m_head, M_NOWAIT, 1);
		if (m == NULL) {
			/* XXX increment counter? */
			m_freem(*m_head);
			*m_head = NULL;
			return (ENOBUFS);
		}
		*m_head = m;
		/* XXX how do we guarantee this won't loop forever? */
		goto retry;
	} else
		/* XXX increment counter? */
		return (error);

	if (nsegs == 0) {
		m_freem(*m_head);
		*m_head = NULL;
		return (EIO);
	}

	/* Make sure we have enough descriptors available. */
	/* XXX what's up with the - 2? It's in em(4) and age(4). */
	if (nsegs > sc->alx_tx_queue.count - 2) {
		/* XXX increment counter? */
		bus_dmamap_unload(sc->alx_tx_tag, sc->alx_tx_dmamap);
		return (ENOBUFS);
	}

	for (i = 0; i < nsegs; i++) {
		first = sc->alx_tx_queue.pidx;
		td = &sc->alx_tx_queue.tpd_hdr[first];
		td->addr = htole64(seg.ds_addr);
		FIELD_SET32(td->word0, TPD_BUFLEN, seg.ds_len);
	}

	return (0);
}

static void
alx_start(struct ifnet *ifp)
{
	struct alx_softc *sc = ifp->if_softc;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	ALX_LOCK(sc);
	alx_start_locked(ifp);
	ALX_UNLOCK(sc);
}

static void
alx_start_locked(struct ifnet *ifp)
{
	struct alx_softc *sc;
	struct mbuf *m_head;

	sc = ifp->if_softc;
	ALX_LOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

	/* XXX check link here. */

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (alx_encap(sc, &m_head)) {
			if (m_head != NULL)
				IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			break;
		}

		/* Let BPF listeners know about this frame. */
		ETHER_BPF_MTAP(ifp, m_head);
	}

	/* XXX start wdog */
}

static void
alx_stop(struct alx_softc *sc)
{
	struct ifnet *ifp;

	ALX_LOCK_ASSERT(sc);

	ifp = sc->alx_ifp;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	alx_intr_disable(sc);

	/* XXX what else? */
}

static void
alx_reset(struct alx_softc *sc)
{
	device_t dev;
	struct alx_hw *hw;
	bool phy_cfged;

	dev = sc->alx_dev;
	hw = &sc->hw;

	alx_reset_pcie(hw);

	phy_cfged = alx_phy_configed(hw);
	if (!phy_cfged)
		alx_reset_phy(hw, !hw->hib_patch);

	if (alx_reset_mac(hw))
		device_printf(dev, "failed to reset MAC\n");

	/* XXX what else? */
}

static void
alx_int_task(void *context __unused, int pending __unused)
{
}

static int
alx_irq_legacy(void *arg)
{
	struct alx_softc *sc;
	struct alx_hw *hw;
	uint32_t intr;

	sc = arg;
	hw = &sc->hw;

	ALX_MEM_R32(hw, ALX_ISR, &intr);
	if (intr & ALX_ISR_DIS || (intr & hw->imask) == 0)
		return (FILTER_STRAY);

	return (FILTER_HANDLED);
}

int
alx_allocate_legacy_irq(struct alx_softc *sc)
{
	device_t dev;
	int rid, error;

	dev = sc->alx_dev;

	sc->nr_txq = 1;
	sc->nr_rxq = 1; // XXX needed?
	sc->nr_vec = 1;
	sc->nr_hwrxq = 1;

	rid = 0;
	sc->alx_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->alx_irq == NULL) {
		device_printf(dev, "cannot allocate IRQ\n");
		return (ENXIO);
	}

	error = bus_setup_intr(dev, sc->alx_irq, INTR_TYPE_NET,
	    alx_irq_legacy, NULL, sc, &sc->alx_cookie);
	if (error != 0) {
		device_printf(dev, "failed to register interrupt handler\n");
		return (ENXIO);
	}

	sc->alx_tq = taskqueue_create_fast("alx_taskq", M_WAITOK,
	    taskqueue_thread_enqueue, &sc->alx_tq);
	if (sc->alx_tq == NULL) {
		device_printf(dev, "could not create taskqueue\n");
		return (ENXIO);
	}
	TASK_INIT(&sc->alx_int_task, 0, alx_int_task, sc);
	taskqueue_start_threads(&sc->alx_tq, 1, PI_NET, "%s taskq",
	    device_get_nameunit(sc->alx_dev));

	return (0);
}

static int
alx_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct alx_softc *sc;
	struct ifreq *ifr;
	int error = 0;

	sc = ifp->if_softc;
	ifr = (struct ifreq *)data;

	switch (command) {
	case SIOCSIFFLAGS:
		ALX_LOCK(sc);
		if ((ifp->if_flags & IFF_UP) &&
		    !(ifp->if_flags & IFF_DRV_RUNNING)) {
			alx_init_locked(sc);
		} else {
			if (!(ifp->if_flags & IFF_DRV_RUNNING))
				alx_stop(sc);
		}
		sc->alx_if_flags = ifp->if_flags;
		ALX_UNLOCK(sc);
		break;
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->alx_media, command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

static int
alx_media_change(struct ifnet *ifp)
{

	return (0);
}

static void
alx_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct alx_softc *sc;
	struct alx_hw *hw;
	uint16_t speed, mode;
	bool link_up;
	int error;

	sc = ifp->if_softc;
	hw = &sc->hw;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	/*
	 * Clear the PHY internal interrupt status. XXX why?
	 */
	alx_clear_phy_intr(hw);

	error = alx_get_phy_link(hw, &link_up, &speed);
	if (error != 0)
		return;

	if (link_up)
		ifmr->ifm_status |= IFM_ACTIVE;
	else
		return;

	mode = speed % 10;
	speed -= mode;

	switch (mode) {
	case ALX_FULL_DUPLEX:
		ifmr->ifm_active |= IFM_FDX;
		break;
	case ALX_HALF_DUPLEX:
		ifmr->ifm_active |= IFM_HDX;
		break;
	default:
		device_printf(sc->alx_dev, "invalid duplex mode %u\n", mode);
		break;
	}

	switch (speed) {
	case SPEED_10:
		ifmr->ifm_active |= IFM_10_T;
		break;
	case SPEED_100:
		ifmr->ifm_active |= IFM_100_TX;
		break;
	case SPEED_1000:
		ifmr->ifm_active |= IFM_1000_T;
		break;
	default:
		device_printf(sc->alx_dev, "invalid link speed %u\n", speed);
		break;
	}
}

static int
alx_probe(device_t dev)
{
	struct alx_dev *alx;
	uint16_t vendor, device;
	int i;

	vendor = pci_get_vendor(dev);
	device = pci_get_device(dev);

	alx = alx_devs;
	for (i = 0; i < ALX_DEV_COUNT; i++, alx++) {
		if (alx->alx_vendorid == vendor &&
		    alx->alx_deviceid == device) {
			device_set_desc(dev, alx->alx_name);
			return (BUS_PROBE_DEFAULT);
		}
	}

	return (ENXIO);
}

static int
alx_attach(device_t dev)
{
	struct alx_softc *sc;
	struct alx_hw *hw;
	struct ifnet *ifp;
	bool phy_cfged;
	int err, rid;

	sc = device_get_softc(dev);
	sc->alx_dev = dev;

	mtx_init(&sc->alx_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);

	pci_enable_busmaster(dev);

	rid = PCIR_BAR(0);
	sc->alx_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->alx_res == NULL) {
		device_printf(dev, "cannot allocate memory resources\n");
		return (ENXIO);
	}
	hw = &sc->hw;
	hw->hw_addr = sc->alx_res;
	hw->dev = dev;

	err = alx_allocate_legacy_irq(sc);
	if (err != 0)
		goto fail;

	err = alx_init_sw(sc);
	if (err != 0) {
		device_printf(dev, "failed to initialize device softc\n");
		err = ENXIO;
		goto fail;
	}

	err = alx_dma_alloc(sc);
	if (err != 0) {
		device_printf(dev, "cannot initialize DMA mappings\n");
		err = ENXIO;
		goto fail;
	}

	alx_reset_pcie(hw);

	phy_cfged = alx_phy_configed(hw);

	if (!phy_cfged)
		alx_reset_phy(hw, !hw->hib_patch);
	err = alx_reset_mac(hw);
	if (err) {
		device_printf(dev, "MAC reset failed with error %d\n", err);
		err = ENXIO;
		goto fail;
	}

	if (!phy_cfged) {
		err = alx_setup_speed_duplex(hw, hw->adv_cfg, hw->flowctrl);
		if (err) {
			device_printf(dev,
			    "failed to configure PHY with error %d\n", err);
			err = ENXIO;
			goto fail;
		}
	}

	err = alx_get_perm_macaddr(hw, hw->perm_addr);
	if (err != 0) {
		/* XXX Generate a random MAC address instead? */
		device_printf(dev, "could not retrieve MAC address\n");
		err = ENXIO;
		goto fail;
	}
	memcpy(&hw->mac_addr, &hw->perm_addr, ETHER_ADDR_LEN);

	sc->alx_ifp = if_alloc(IFT_ETHER);
	if (sc->alx_ifp == NULL) {
		device_printf(dev, "failed to allocate an ifnet\n");
		err = ENOSPC;
		goto fail;
	}

	ifp = sc->alx_ifp;
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST; /* XXX */
	ifp->if_capabilities = IFCAP_HWCSUM; /* XXX others? */
	ifp->if_ioctl = alx_ioctl;
	ifp->if_start = alx_start;
	ifp->if_init = alx_init;

	ether_ifattach(ifp, hw->mac_addr);

	ifmedia_init(&sc->alx_media, IFM_IMASK, alx_media_change,
	    alx_media_status);
	ifmedia_add(&sc->alx_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_add(&sc->alx_media, IFM_ETHER | IFM_10_T, 0, NULL);
	ifmedia_add(&sc->alx_media, IFM_ETHER | IFM_10_T | IFM_FDX, 0, NULL);
	ifmedia_add(&sc->alx_media, IFM_ETHER | IFM_100_TX, 0, NULL);
	ifmedia_add(&sc->alx_media, IFM_ETHER | IFM_100_TX | IFM_FDX, 0, NULL);
	if (pci_get_device(dev) & 1) {
		/* GigE-capable chipsets have an odd device ID. */
		ifmedia_add(&sc->alx_media, IFM_ETHER | IFM_1000_T, 0, NULL);
		ifmedia_add(&sc->alx_media, IFM_ETHER | IFM_1000_T | IFM_FDX, 0,
		    NULL);
	}
	ifmedia_set(&sc->alx_media, IFM_ETHER | IFM_AUTO);

#if 0
	netdev->netdev_ops = &alx_netdev_ops;
	alx_set_ethtool_ops(netdev);
	netdev->irq  = pdev->irq;
	netdev->watchdog_timeo = ALX_WATCHDOG_TIME;

	/* PHY mdio */
	hw->mdio.prtad = 0;
	hw->mdio.mmds = 0;
	hw->mdio.dev = netdev;
	hw->mdio.mode_support =
		MDIO_SUPPORTS_C45 | MDIO_SUPPORTS_C22 | MDIO_EMULATE_C22;
	hw->mdio.mdio_read = alx_mdio_read;
	hw->mdio.mdio_write = alx_mdio_write;
	if (!alx_get_phy_info(hw)) {
		device_printf(dev, "identify PHY failed\n");
		err = -EIO;
		goto err_id_phy;
	}
#endif

fail:
	if (err != 0)
		alx_detach(dev);

	return (err);
}

static int
alx_detach(device_t dev)
{
	struct alx_softc *sc;
	struct alx_hw *hw;

	sc = device_get_softc(dev);
	hw = &sc->hw;

	ALX_FLAG_SET(sc, HALT);
	if (device_is_attached(dev)) {
	}

	/* Restore permanent mac address. */
	alx_set_macaddr(hw, hw->perm_addr);

	bus_generic_detach(dev);
	/* XXX Free DMA */

	ether_ifdetach(sc->alx_ifp);
	if (sc->alx_ifp != NULL) {
		if_free(sc->alx_ifp);
		sc->alx_ifp = NULL;
	}

	if (sc->alx_tq != NULL) {
		taskqueue_drain(sc->alx_tq, &sc->alx_int_task);
		taskqueue_free(sc->alx_tq);
	}

	if (sc->alx_cookie != NULL)
		bus_teardown_intr(dev, sc->alx_irq, sc->alx_cookie);

	if (sc->alx_irq != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->alx_irq);

	if (sc->alx_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(0),
		    sc->alx_res);

	mtx_destroy(&sc->alx_mtx);

	return (0);
}

static int
alx_shutdown(device_t dev)
{

	return (0);
}

static int
alx_suspend(device_t dev)
{

	return (0);
}

static int
alx_resume(device_t dev)
{

	return (0);
}
