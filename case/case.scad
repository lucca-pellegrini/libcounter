// libcounter enclosure
// Two-part case (body + sliding lid) for a full-size 830 point breadboard
// carrying the ESP32-S3 DevKit, SSD1306 OLED, a push button, a 12V DC jack and
// an ultrasonic sensor cable.
//
// The breadboard is mounted with its component side facing the FRONT (the
// floor of the body), so the OLED looks out through the window in the floor.
// The back of the breadboard faces the lid.
//
//   Body seen from the front (outside of the floor), X to the right:
//
//   +-----------------------------------------------------+
//   |                                                     |
//   |  12V      [ screen ]  []button               sensor |
//   |  jack                                        cable  |
//   |                                                     |
//   +-----------------------------------------------------+   <- lid slides in
//                                                                from this long
//                                                                side (+Y)
//
// The lid is a "drawer": it slides along rails in the two SHORT end walls
// (along Y), entering from the +Y long side only. The opposite long wall (-Y)
// is the stop. The lid stays inside the outline of the body and has a grooved
// grip texture on its outer face.
//
// Printing: render with part = "base" and part = "lid" and export each as STL.
// Both parts are oriented ready to print (no supports needed).
// Everything is in millimetres.

/* [Part to render] */
// "base", "lid", "plate" (both parts side by side, for a single 3MF) or
// "assembly" (preview only, lid shown in place)
part = "assembly"; // [base, lid, plate, assembly]

/* [Breadboard] */
bb_length    = 165.1;  // full-size breadboard, long side
bb_width     = 54.6;   // full-size breadboard, short side
bb_thickness = 9.0;    // incl. adhesive backing
bb_clearance = 0.6;    // gap on each side so it fits after printing

/* [Case] */
wall             = 2.4;  // side wall thickness
floor_t          = 2.4;  // front plate (floor) thickness
component_height = 18;   // height of the 4 wall pads = space under the breadboard. Keep LOW:
                         // the breadboard rests on these, so taller pads = less room above
top_height       = 20;   // free height ABOVE the breadboard back (power module etc.), up to the lid
bb_slack         = 0.4;  // extra play between breadboard back and lid
end_zone_jack    = 14;   // free space between breadboard and the 12V jack wall (-X)
end_zone_cable   = 8;    // free space between breadboard and the sensor cable wall (+X)

/* [Sliding lid] */
lid_t        = 3.5;    // lid thickness (also the height of the slide rails)
lid_vert     = 1;      // vertical part of the lid edge, the rest is a 45 deg chamfer
ledge_w      = 3;      // width of the ledge the lid rests on
lid_gap      = 0.35;   // clearance between lid and rails (increase if too tight)
lid_end_gap  = 0.3;    // clearance between lid and the closed (-Y) wall
slide        = 0;      // assembly preview only: how far the lid is pulled out (+Y)

/* [Grip texture on lid] */
grip_pitch   = 2.2;    // distance between grooves
grip_w       = 1.2;    // groove width at the surface
grip_depth   = 0.6;    // groove depth (V shaped)
grip_margin  = 4;      // untextured border around the lid face

/* [Screen] (in the front plate, positions from its centre) */
screen_w     = 24.7;   // along X
screen_h     = 16.6;   // along Y
screen_x     = 0;
screen_y     = 0;

/* [Push button] (in the front plate) */
button_size  = 6;      // square hole
button_gap   = 8;      // distance between screen edge and button hole edge
// button sits toward +X (sensor side) of the screen; change to put it elsewhere
button_x     = screen_x + screen_w / 2 + button_gap + button_size / 2;
button_y     = screen_y;

/* [12V DC jack] (left end wall, -X) */
jack_d       = 11;     // hole diameter (adjust to your jack)
jack_y       = 0;      // offset along Y
jack_z       = 11;     // centre height above the breadboard surface (hole top must stay under the lid ledge)

/* [Sensor cable] (right end wall, +X) */
cable_d      = 10;
cable_y      = 0;
cable_z      = 8;      // centre height above the breadboard surface

/* [Breadboard holders] */
pad_reach    = 2.5;    // how far the support pads reach in from the long walls
pad_len      = 12;     // pad length along X
pad_x        = 30;     // pads sit at +-pad_x from the breadboard centre, kept well
                       // away from the ends where the power module plugs in

$fn = 64;
eps = 0.01;

// ---- derived ---------------------------------------------------------------
in_l    = bb_length + 2 * bb_clearance + end_zone_jack + end_zone_cable;  // interior X
in_w    = bb_width  + 2 * bb_clearance;                 // interior Y
out_l   = in_l + 2 * wall;
out_w   = in_w + 2 * wall;
Xi      = in_l / 2;                                     // interior half length
bb_cx   = (end_zone_jack - end_zone_cable) / 2;         // breadboard centre X
Yi      = in_w / 2;                                     // interior half width
bb_front_z = floor_t + component_height;                // breadboard front face
bb_back_z  = bb_front_z + bb_thickness;                 // breadboard top surface
zl      = bb_back_z + bb_slack + top_height;            // ledge top / lid bottom
total_h = zl + lid_t;                                   // top of walls = top of lid
lid_y0  = -Yi + lid_end_gap;                            // closed end of lid
lid_y1  = out_w / 2;                                    // open end, flush with body

module rounded_box(l, w, h, r = 2) {
    hull()
        for (sx = [-1, 1], sy = [-1, 1])
            translate([sx * (l / 2 - r), sy * (w / 2 - r), 0])
                cylinder(r = r, h = h);
}

// Extrude a 2D profile given as [x, z] points along Y from y0 to y1.
module along_y(y0, y1) {
    translate([0, y1, 0]) rotate([90, 0, 0])
        linear_extrude(height = y1 - y0) children();
}

// ---- body ------------------------------------------------------------------
// Slide rails (per short end wall, +X side shown, mirrored for -X):
//   * ledge: horizontal shelf the lid rests on, 45 deg gusset underneath
//   * rail : triangle above the lid edge, 45 deg underside, holds the lid in
// Both are printable without supports. The +Y long wall is cut down to the
// ledge so the lid slides in from that side only; the -Y wall is the stop.
module rails() {
    for (m = [0, 1]) mirror([m, 0, 0])
        along_y(-Yi - 0.5, out_w / 2) {
            // ledge
            polygon([[Xi + 0.5, zl - ledge_w], [Xi, zl - ledge_w],
                     [Xi - ledge_w, zl], [Xi + 0.5, zl]]);
            // hold-down rail
            polygon([[Xi + 0.5, zl + lid_vert], [Xi, zl + lid_vert],
                     [Xi - (lid_t - lid_vert), total_h],
                     [Xi + 0.5, total_h]]);
        }
}

module base_part() {
    difference() {
        union() {
            difference() {
                rounded_box(out_l, out_w, total_h);
                // interior
                translate([-Xi, -Yi, floor_t])
                    cube([in_l, in_w, total_h]);
                // open side: lower the +Y wall to just below the lid
                translate([-Xi, Yi - 0.01, zl - 0.2])
                    cube([in_l, wall + 1, lid_t + 1]);
            }
            rails();

            // support pads: breadboard front face rests on these
            for (sx = [-1, 1], sy = [-1, 1])
                translate([bb_cx + sx * pad_x - pad_len / 2,
                           sy > 0 ? Yi - pad_reach : -Yi - 0.5, floor_t - eps])
                    cube([pad_len, pad_reach + 0.5, component_height + eps]);
        }

        // screen window
        translate([screen_x - screen_w / 2, screen_y - screen_h / 2, -1])
            cube([screen_w, screen_h, floor_t + 2]);

        // push button hole
        translate([button_x - button_size / 2, button_y - button_size / 2, -1])
            cube([button_size, button_size, floor_t + 2]);

        // 12V DC jack hole, -X wall
        translate([-out_l / 2 - 1, jack_y, bb_back_z + jack_z])
            rotate([0, 90, 0]) cylinder(d = jack_d, h = wall + 2);

        // sensor cable hole, +X wall
        translate([out_l / 2 - wall - 1, cable_y, bb_back_z + cable_z])
            rotate([0, 90, 0]) cylinder(d = cable_d, h = wall + 2);
    }
}

// ---- lid -------------------------------------------------------------------
// Local coordinates: bottom at z = 0, Y in case coordinates (slides along +Y).
// Printed as modelled: the wide underside sits on the bed, no supports.
module lid_part() {
    hw  = Xi - lid_gap;                 // half width at the bottom
    hwt = hw - (lid_t - lid_vert);      // half width at the top
    gx  = hwt - grip_margin;            // grooves run over |x| < gx
    n   = floor((lid_y1 - lid_y0 - 2 * grip_margin) / grip_pitch);
    gy0 = (lid_y0 + lid_y1) / 2 - (n - 1) * grip_pitch / 2;
    difference() {
        along_y(lid_y0, lid_y1)
            polygon([[-hw, 0], [hw, 0], [hw, lid_vert], [hwt, lid_t],
                     [-hwt, lid_t], [-hw, lid_vert]]);

        // grip grooves (recessed, so the lid never rises above the body)
        for (i = [0 : n - 1])
            translate([-gx, gy0 + i * grip_pitch, 0])
                rotate([90, 0, 90]) linear_extrude(height = 2 * gx)
                    polygon([[-grip_w / 2, lid_t + eps],
                             [0, lid_t - grip_depth],
                             [grip_w / 2, lid_t + eps]]);
    }
}

// ---- output ----------------------------------------------------------------
if (part == "base") {
    base_part();
} else if (part == "lid") {
    lid_part();
} else if (part == "plate") {
    // both parts laid out next to each other on one print bed
    translate([0, -(out_w / 2 + 3), 0]) base_part();
    translate([0, 3 - lid_y0, 0])
        lid_part();
} else {
    base_part();
    translate([0, slide, zl]) color("lightblue") lid_part();
}
