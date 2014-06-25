import os, re

mehari_private = os.environ["MEHARI_PRIVATE"]

if not os.path.exists(mehari_private) or not os.path.exists(mehari_private + "/examples/dc_motor/dc_motor.c"):
	raise Exception("Please set the environment variable MEHARI_PRIVATE to the private mehari repository.")

examples = {
	"Half Axis":                       "/home/benny/mehari-private/examples/Halbachse/Halbachse.c",
	"Woodpecker (optimized)":          "/home/benny/mehari-private/examples/woodpecker/woodpecker_opt.c",
	"Woodpecker":                      "/home/benny/mehari-private/examples/woodpecker/woodpecker.c",
	"DC Motor":                        "/home/benny/mehari-private/examples/dc_motor/dc_motor.c",
	"Single Pendulum":                 "/home/benny/mehari-private/examples/single_pendulum/single_pendulum.c",
	"Lunar Landing":                   "/home/benny/mehari-private/examples/lunar_landing/lunar_landing.c",
	"Lunar Landing (optimized)":       "/home/benny/mehari-private/examples/lunar_landing/lunar_landing_opt.c",
	"Two Masses Bouncing":             "/home/benny/mehari-private/examples/two_masses_bouncing/two_masses_bouncing.c",
	"Two Masses Bouncing (optimized)": "/home/benny/mehari-private/examples/two_masses_bouncing/two_masses_bouncing_opt.c"
}

def clean_for_filename(name):
	return re.sub(r"[^a-zA-Z0-9_\-]+", "_", name)
