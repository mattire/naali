# imitated loadurlhandler here, for direct access from C++ side

try:
    localscene #err, this embedded context is weird?
except: #first run
    import localscene
else:
    localscene = reload(localscene)

from localscene import LocalScene, getLocalScene

