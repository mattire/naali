"""the modules that are to be loaded when the viewer starts.
add your module *class* (the circuits Component) to a .ini file in this directory.
"""
import rexviewer as r
import sys, os
import traceback
from glob import glob
from ConfigParser import ConfigParser

from core.freshimport import freshimport

def read_inis():
    thisdir = os.path.split(os.path.abspath(__file__))[0]
    for inifile in glob(os.path.join(thisdir, "*.ini")):
        cp = ConfigParser()
        cp.read([inifile])
        for s in cp.sections():
            cfdict = dict(cp.items(s))
            try:
                modname, compname = s.rsplit('.', 1)
            except ValueError:
                r.logInfo("bad config section in " + inifile + ": " + s)
                continue

            yield modname, compname, cfdict
        
modules = []
for modname, compname, cfg in read_inis():
    m = freshimport(modname)
    if m is not None: #loaded succesfully. if not, freshload logged traceback
        c = getattr(m, compname)
        modules.append((c, cfg))

def load(circuitsmanager):
    for klass, cfg in modules:
        #~ modinst = klass()
        #~ circuitsmanager += modinst
        #print klass
        try:
            if cfg:
                modinst = klass(cfsection=cfg)
            else:
                modinst = klass()
        except Exception, exc:
            r.logInfo("failed to instantiate pymodule %s" % klass)
            r.logInfo(traceback.format_exc())
        else:
            circuitsmanager += modinst # Equivalent to: tm.register(m)

    #del modules #attempt to improve reloading, not keep refs to old versions
