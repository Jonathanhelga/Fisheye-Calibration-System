# PCT and ICT, explained simply

If you have ever felt lost seeing "PCT" and "ICT" thrown around in the calibration tables and graphs, this page is for you.
No fancy terms, just plain language and a simple picture to hold in your head.

## The one picture to remember

Imagine you draw a bullseye target on a big piece of paper.
You draw the rings very carefully with a ruler, so you know exactly how far out each ring is.
Say the first ring is 10 millimeters from the center, the second is 20 millimeters, the third is 30 millimeters, and so on.

You wrote those numbers down before anything else happened.
Nobody measured them from a photo.
You know them because you drew them yourself.

Now you take that bullseye and photograph it through a fisheye camera lens.
Fisheye lenses are the ones that bulge things outward, like looking through a peephole in a door or a security camera's wide view.

When you look at the photo afterward, those rings do not look evenly spaced anymore.
The lens bent the light, so a ring that was a clean, even distance on paper now looks squished or stretched depending on which direction you look from the center.

So now you have two very different sets of numbers to compare:

1. What you actually drew on paper (the truth, before any camera touched it)
2. What the camera's photo shows after the fisheye lens bent everything

That comparison, between the truth and what the lens did to it, is the whole idea behind camera calibration.
PCT is the first set of numbers.
ICT is the second.

## PCT, the "what we actually drew" numbers

PCT stands for the pattern's own measurements, the ones that exist purely on paper, before a camera is even involved.

Think of PCT as the answer key.
It is not a guess, and it is not measured from anything blurry or uncertain.
It comes straight from how the calibration pattern was designed.

In this project, the calibration pattern is made of two kinds of shapes:

- Concentric rings, like the bullseye we imagined above. There can be up to 25 of these rings, each one further out than the last.
- Stripe lines, thin lines spaced out from the center. There can be up to 50 of these.

Put those together and you get 75 numbers total.
Every single one of them is a known, physical distance that was typed in and used to generate the printed (or on screen) pattern image in the first place.

There is a tool in this software called the Pattern Generator that is in charge of creating this pattern and remembering all 75 of those distances.
So whenever you hear "PCT," just think:

> "The distances we already know for certain, because we designed them ourselves."

## ICT, the "what the camera actually saw" numbers

ICT is the opposite side of the comparison.
It stands for the distances measured from the photo itself, after the fisheye lens has already bent the light.

Here is the important part.
You take a picture of that same bullseye pattern through the camera you are trying to calibrate.
Then, in the photo, you measure how far out each ring or stripe actually appears, this time counted in pixels instead of millimeters, since a digital photo is made of pixels.

You might notice that ICT is sometimes also called IH in the documentation.
Both names mean the exact same thing.
It is just two different names people used for the same measurement over time.

There is one more twist that makes ICT more detailed than PCT.
A fisheye lens does not bend every direction the same amount.
Straight up might bend differently than straight sideways, and a diagonal direction bends differently again.

So instead of one single number per ring, the software actually measures ICT in eight separate directions around the center:

- North, South, East, West (up, down, right, left)
- Northeast, Northwest, Southeast, Southwest (the four diagonal directions in between)

That is why you will see columns labeled things like ICT N, ICT S, ICT W, ICT E, ICT NW, ICT SE, ICT SW, and ICT NE in the calibration tables.
Each one is asking the same simple question, just pointed in a different direction:

> "In this direction, how many pixels out did that same ring actually land, once the lens got its hands on it?"

## Why comparing PCT and ICT matters

Once you have both numbers side by side, the real work of calibration begins.

For any single ring or stripe, you now know:

- PCT: how far it really is, in real world distance (say, 20 millimeters)
- ICT: how far it appears in the photo, in pixels (say, 148 pixels), and this can be a different pixel count depending on direction

If a lens had absolutely no distortion at all, the relationship between PCT and ICT would be the same simple straight ratio in every direction.
But a fisheye lens is deliberately curved and bulging, so the relationship bends and curves too, and it bends differently in different directions.

By collecting many PCT and ICT pairs, from many rings, many stripes, and all eight directions, the software can build a curve that describes exactly how this particular camera and lens bends light.
That curve becomes the calibration result, the thing that lets software later "undo" the fisheye bending and turn a bulged image back into something that looks normal and straight.

## A shorter way to remember it

If the long explanation above ever feels like too much at once, here is the short version you can keep in your back pocket:

- PCT: the truth. What we drew, measured in real distance, before any camera was involved.
- ICT (same thing as IH): what the camera saw. Measured in pixels, from the actual photo, after the fisheye lens bent it.
- Calibration is simply comparing those two things, over and over, in eight directions, across many rings and stripes, until the software understands exactly how this lens bends light.

That comparison is quite literally what almost every table, graph, and curve in the calibration result window is built from.
