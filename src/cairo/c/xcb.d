/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2009 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

module cairo.c.xcb;

import cairo.c.cairo;

version(CAIRO_HAS_XCB_SURFACE)
{
    pragma(msg, "cairo.c.xcb: FIX: need proper xcb bindings");
    //import xcb.xcb;
    //import xcb.render;
    alias void xcb_connection_t;
    alias void xcb_screen_t;
    alias void xcb_visualtype_t;
    alias void xcb_render_pictforminfo_t;
    alias uint xcb_drawable_t;
    alias uint xcb_pixmap_t;

    extern (C):
    cairo_surface_t* 
    cairo_xcb_surface_create (xcb_connection_t* connection,
                  xcb_drawable_t	 drawable,
                  xcb_visualtype_t* visual,
                  int			 width,
                  int			 height);
    
    cairo_surface_t* 
    cairo_xcb_surface_create_for_bitmap (xcb_connection_t* connection,
                         xcb_screen_t* screen,
                         xcb_pixmap_t	 bitmap,
                         int		 width,
                         int		 height);
    
    cairo_surface_t* 
    cairo_xcb_surface_create_with_xrender_format (xcb_connection_t* connection,
                              xcb_screen_t* screen,
                              xcb_drawable_t			 drawable,
                              xcb_render_pictforminfo_t* format,
                              int				 width,
                              int				 height);
    
    void
    cairo_xcb_surface_set_size (cairo_surface_t* surface,
                    int		     width,
                    int		     height);
    
    /* debug interface */
    
    void
    cairo_xcb_device_debug_cap_xshm_version (cairo_device_t *device,
                                             int major_version,
                                             int minor_version);
    
    void
    cairo_xcb_device_debug_cap_xrender_version (cairo_device_t *device,
                                                int major_version,
                                                int minor_version);
}
else
{
    //static assert(false, "Cairo was not compiled with support for the xcb backend");
}
