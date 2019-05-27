#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
/* FIXME */
#include "/sys/src/cmd/jpg/imagefile.h"
#include "fns.h"

extern int debug;

typedef struct Header Header;
struct Header{
	u8int id;
	u8int ver;
	u8int bpp;
	u16int xmin;
	u16int ymin;
	u16int xmax;
	u16int ymax;
	u8int egapal[48];
	u8int np;
	u16int bpl;
	u16int ptyp;
};

enum{
	Hdrsz = 128
};


static void *
emalloc(ulong n)
{
	void *p;

	p = mallocz(n, 1);
	if(p == nil)
		sysfatal("mallocz: %r");
	return p;
}

static void
readheader(Biobuf *b, Header *h)
{
	uchar buf[Hdrsz];

	if(Bread(b, buf, sizeof buf) != sizeof buf)
		sysfatal("ReadPCX: can't read header: %r");
	h->id = buf[0];
	if(h->id != 10)
		sysfatal("ReadPCX: bad id %ux", h->id);
	h->ver = buf[1];
	h->bpp = buf[3];
	h->xmin = buf[4] | buf[5] << 8;
	h->ymin = buf[6] | buf[7] << 8;
	h->xmax = buf[8] | buf[9] << 8;
	h->ymax = buf[10] | buf[11] << 8;
	memcpy(h->egapal, buf+16, sizeof h->egapal);
	h->np = buf[65];
	h->bpl = buf[66] | buf[67] << 8;
	h->ptyp = buf[68] | buf[69] << 8;
}

static Rawimage*
readslave(Biobuf *b)
{
	int w, h, t, hlen, pad;
	uint x;
	char c;
	uchar pal[3*256], *p, *end;
	Rawimage *r;
	Header *hd;

	hd = emalloc(sizeof *hd);
	readheader(b, hd);

	w = hd->xmax - hd->xmin + 1;
	h = hd->ymax - hd->ymin + 1;
	t = w * h;
	hlen = hd->np * hd->bpl;
	pad = 8 / hd->bpp * hlen - w;
	if(debug)
		fprint(2, "pcx: v.%d pt.%d %dx%dx%d in %dx%d, %d padded\n",
			hd->ver, hd->ptyp, w, h, hd->bpp, hd->np, hd->bpl, pad);

	r = emalloc(sizeof *r);
	r->r = Rect(0, 0, w, h);

	switch(hd->ver){
	case 5:
		t *= 3;
		r->nchans = 1;
		r->chandesc = CRGB24;
		r->chanlen = t;
		r->chans[0] = emalloc(t);
		p = r->chans[0];
		end = p + t;
		while(p < end){
			x = 1;
			c = Bgetc(b);
			if((c & 0xc0) == 0xc0){
				x = c & 0x3f;
				c = Bgetc(b);
			}
			while(x-- > 0 && p < end){
				*p = c;
				p += 3;
			}
		}

		Bseek(b, -769, 2);
		if(Bgetc(b) == 0xc){
			if(Bread(b, pal, sizeof pal) != sizeof pal)
				sysfatal("ReadPCX: bad vga palette");
			p = r->chans[0];
			while(p < end){
				x = *p;
				*p++ = pal[x*3+2];
				*p++ = pal[x*3+1];
				*p++ = pal[x*3];
			}
		}
		break;
	default:	/* FIXME: other pcx types */
		sysfatal("ReadPCX: unsupported version %d\n", hd->ver);
	}

	free(hd);
	return r;
}

Rawimage**
Breadpcx(Biobuf *b, int colorspace)
{
	Rawimage **ra;

	if(colorspace != CRGB){
		werrstr("ReadPCX: unknown color space %d", colorspace);
		return nil;
	}
	ra = emalloc(2 * sizeof *ra);	/* why */

	ra[0] = readslave(b);
	ra[1] = nil;
	return ra;
}
