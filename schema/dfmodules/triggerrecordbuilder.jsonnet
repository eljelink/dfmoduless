local moo = import "moo.jsonnet";
local ns = "dunedaq.dfmodules.triggerrecordbuilder";
local s = moo.oschema.schema(ns);

local types = {
    apanumber : s.number("apa_number", "u4",
                     doc="APA number for the mapping of data request queues"),	
    linknumber : s.number("link_number", "u4",
                     doc="Link number for the mapping of data request queues"),

    queueid : s.string("queue_id", doc="Parameter that configure RequestGenerator queue module"),

    geoidqueue : s.record("geoidinst", [s.field("apa", self.apanumber, doc="" ) , 
                                        s.field("link", self.linknumber, doc="" ), 
                                        s.field("queueinstance", self.queueid, doc="" ) ], 
                           doc="RequestGenerator configuration"),

    mapgeoidqueue : s.sequence("mapgeoidqueue",  self.geoidqueue, doc="Map of geoids queues" ),

    timestamp_diff: s.number( "TimestampDiff", "i8", 
                              doc="A timestamp difference" ),

    queue_timeout: s.number( "QueueTimeout", "u4", 
                             doc="Queue timeout in milliseconds" ),    
  
    conf: s.record("ConfParams", [ s.field("map", self.mapgeoidqueue, doc="" ), 
                                   s.field("general_queue_timeout", self.queue_timeout, 100, 
                                           doc="General indication for timeout"),
                                   s.field("max_timestamp_diff", self.timestamp_diff, 50000000, 
                                           doc="General indication for timeout to throw errors")
                                  ] , 
                   doc="TriggerRecordBuilder configuration")

};

moo.oschema.sort_select(types, ns)