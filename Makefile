KMOD=	if_alx
SRCS=	if_alx.c device_if.h bus_if.h pci_if.h

SRCS+=	alx_hw.c compat.c

.include <bsd.kmod.mk>
