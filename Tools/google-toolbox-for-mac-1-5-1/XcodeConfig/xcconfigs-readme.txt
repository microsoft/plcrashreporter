Xcode Config is sorta a black art, any time you have a set of rules, you
quickly hit a few exceptions.

The main goal of using these is as follow:

Edit your Project level build settings by removing as much as possible, and
then set the per Configuration settings to one of the project xcode config
files w/in the Project subfolder here.  This will batch setup the project to
build Debug/Release w/ a specific SDK.

If you are building a Shared Library, Loadable Bundle (Framework) or UnitTest
you will need to apply a further Xcode Config file at the target level.  You do
this again by clearing most of the settings on the target, and just setting the
build config for that target to be the match from the Target subfolder here.

To see an example of this, look at CoverStory
(http://code.google.com/p/coverstory) or Vidnik
(http://code.google.com/p/vidnik).


The common exception...If you need to have a few targets build w/ different
SDKs, then you hit the most common of the exceptions.  For these, you'd need
the top level config not to set some things, the simplest way to do this seems
to be to remove as many of the settings from the project file, and make new
wrapper xcconfig files that inclue both the project level and target level
setting and set them on the targets (yes, this is like the MetroWerks days
where you can quickly explode in a what seems like N^2 (or worse) number of
config files.  With a little luck, future versions of Xcode might have some
support to make mixing SDKs easier.

Remember: When using the configs at any given layer, make sure you set them for
each build configuration you need (not just the active one).
