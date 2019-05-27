#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>
/* FIXME */
#include "/sys/src/cmd/jpg/imagefile.h"
#include "fns.h"

enum{
	Border = 2
};

int dflag;
int nineflag;
int debug;
Image *image;


static Rectangle
imager(Image *i)
{
	Point p1, p2;

	p1 = addpt(divpt(subpt(i->r.max, i->r.min), 2), i->r.min);
	p2 = addpt(divpt(subpt(screen->clipr.max, screen->clipr.min), 2), screen->clipr.min);
	return rectaddpt(i->r, subpt(p2, p1));
}

void
eresized(int new)
{
	Rectangle r;

	if(new && getwindow(display, Refnone) < 0){
		fprint(2, "pcx: can't reattach to window\n");
		exits("resize");
	}
	if(image == nil)
		return;
	r = imager(image);
	border(screen, r, -Border, nil, ZP);
	draw(screen, r, image, nil, image->r.min);
	flushimage(display, 1);
}

static char *
show(int fd, char *name)
{
	int c, j;
	char s[9];
	Rawimage **ra, *r;
	Biobuf b;
	Image *i, *i2;

	if(Binit(&b, fd, OREAD) < 0)
		return nil;
	ra = Breadpcx(&b, CRGB);
	if(ra == nil || ra[0] == nil){
		fprint(2, "pcx: decode %s failed: %r\n", name);
		return "decode";
	}
	Bterm(&b);

	r = ra[0];
	/* FIXME: set colorchan for given format (from readpcx) */

	if(!dflag){
		i = allocimage(display, r->r, RGB24, 0, 0);
		if(i == nil){
			fprint(2, "pcx: allocimage %s failed: %r\n", name);
			return "allocimage";
		}
		if(loadimage(i, i->r, r->chans[0], r->chanlen) < 0){
			fprint(2, "pcx: loadimage %s of %d bytes failed: %r\n",
				name, r->chanlen);
			return "loadimage";
		}
		i2 = allocimage(display, r->r, RGB24, 0, 0);
		draw(i2, i2->r, display->black, nil, ZP);
		draw(i2, i2->r, i, nil, i->r.min);
		image = i2;
		eresized(0);
		if((c = ekbd()) == 'q' || c == Kdel || c == Keof)
			exits(nil);
		draw(screen, screen->clipr, display->white, nil, ZP);
		image = nil;
		freeimage(i);
	}
	if(nineflag){
		print("%11s %11d %11d %11d %11d ",
		chantostr(s, RGB24), 0, 0, r->r.max.x, r->r.max.y);
		write(1, r->chans[0], r->chanlen);
	}
	for(j=0; j<r->nchans; j++)
		free(r->chans[j]);
	free(r->cmap);
	free(r);
	free(ra);
	return nil;
}

void
main(int argc, char *argv[])
{
	int i, fd;
	char *err;

	ARGBEGIN{
	case 'D':
		debug++;
		break;
	case 'd':		/* suppress display of image */
		dflag++;
		break;
	case 't':
		break;
	case '9':
		nineflag++;
		dflag++;
		break;
	default:
		fprint(2, "usage: pcx [-t9] [file.pcx ...]\n");
		exits("usage");
	}ARGEND;

	if(!dflag){
		if(initdraw(nil, nil, nil) < 0){
			fprint(2, "pcx: initdraw failed: %r\n");
			exits("initdraw");
		}
		einit(Ekeyboard|Emouse);
	}

	err = nil;
	if(argc == 0)
		err = show(0, "<stdin>");
	else{
		for(i=0; i<argc; i++){
			fd = open(argv[i], OREAD);
			if(fd < 0){
				fprint(2, "pcx: can't open %s: %r\n", argv[i]);
				err = "open";
			}else{
				err = show(fd, argv[i]);
				close(fd);
			}
		}
	}
	exits(err);
}
