"""
 Distance Vector router for CS 168
"""
import sim.api as api
import sim.basics as basics

from dv_utils import PeerTable, PeerTableEntry, ForwardingTable, \
    ForwardingTableEntry

# We define infinity as a distance of 16.
INFINITY = 16

# A route should time out after at least 15 seconds.
ROUTE_TTL = 15


class DVRouter(basics.DVRouterBase):
    # NO_LOG = True  # Set to True on an instance to disable its logging.
    # POISON_MODE = True  # Can override POISON_MODE here.
    # DEFAULT_TIMER_INTERVAL = 5  # Can override this yourself for testing.

    def __init__(self):
        """
        Called when the instance is initialized.

        DO NOT remove any existing code from this method.
        """
        self.start_timer()  # Starts calling handle_timer() at correct rate.

        # Maps a port to the latency of the link coming out of that port.
        self.link_latency = {}

        # Maps a port to the PeerTable for that port.
        # Contains an entry for each port whose link is up, and no entries
        # for any other ports.
        self.peer_tables = {}
        self.route_hist = {}

        # Forwarding table for this router (constructed from peer tables).
        self.forwarding_table = ForwardingTable()

    def add_static_route(self, host, port):
        """
        Adds a static route to a host directly connected to this router.

        Called automatically by the framework whenever a host is connected
        to this router.

        :param host: the host.
        :param port: the port that the host is attached to.
        :returns: nothing.
        """
        # `port` should have been added to `peer_tables` by `handle_link_up`
        # when the link came up.
        assert port in self.peer_tables, "Link is not up?"
        pte = PeerTableEntry(host, 0, PeerTableEntry.FOREVER)
        self.peer_tables[port][host] = pte
        self.update_forwarding_table()

    def handle_link_up(self, port, latency):
        """
        Called by the framework when a link attached to this router goes up.

        :param port: the port that the link is attached to.
        :param latency: the link latency.
        :returns: nothing.
        """
        self.link_latency[port] = latency
        self.peer_tables[port] = PeerTable()
        for dst, fte in self.forwarding_table.items():
            packet = basics.RoutePacket(fte.dst, fte.latency)
            self.send(packet, port)
    #called by the framework when a link attached to this router does down.

    def handle_link_down(self, port):
        """
        Called by the framework when a link attached to this router does down.

        :param port: the port number used by the link.
        :returns: nothing.
        """
        self.link_latency[port] = INFINITY
        for dst, entry in self.peer_tables[port].items():
            self.peer_tables[port][dst] = PeerTableEntry(dst, INFINITY, entry.expire_time)
        self.update_forwarding_table()
        self.send_routes()
    def handle_route_advertisement(self, dst, port, route_latency):
        """
        Called when the router receives a route advertisement from a neighbor.

        :param dst: the destination of the advertised route.
        :param port: the port that the advertisement came from.
        :param route_latency: latency from the neighbor to the destination.
        :return: nothing.

        """
        pte = PeerTableEntry(dst,route_latency,api.current_time()+ ROUTE_TTL)
        self.peer_tables[port][dst] = pte
        self.update_forwarding_table()
        self.send_routes()



    def update_forwarding_table(self):
        """
         Computes and stores a new forwarding table merged from all peer tables.
         :returns: nothing.
        """
        self.forwarding_table.clear()  # First, clear the old forwarding table.
        # TODO: populate `self.forwarding_table` by combining peer tables.
        for port, peer_table in self.peer_tables.items():
            # if self.link_latency[port] < INFINITY:
            for host, entry in peer_table.items():
                if host not in self.forwarding_table:
                    self.forwarding_table[host] = ForwardingTableEntry(host, port, min(INFINITY, entry.latency +
                                                                                       self.link_latency[port]))
                elif entry.latency + self.link_latency[port] < self.forwarding_table[host].latency:
                    self.forwarding_table[host] = ForwardingTableEntry(host, port, min(INFINITY, entry.latency +
                                                                                           self.link_latency[port]))
#forwards the packets to the or drop if the no destination
    def handle_data_packet(self, packet, in_port):

        """
        Called when a data packet arrives at this router.

        You may want to forward the packet, drop the packet, etc. here.

        :param packet: the packet that arrived.
        :param in_port: the port from which the packet arrived.
        :return: nothing.

        """
        packet_dst = packet.dst
        if packet_dst in self.forwarding_table and self.forwarding_table[packet_dst].latency < INFINITY and \
                in_port != self.forwarding_table[packet_dst].port:
            self.send(packet,self.forwarding_table[packet_dst].port)
        else:
            self.send(packet,None)
    #Send route advertisements for all routes in the forwarding table.
    def send_routes(self, force=False):
        """
        :param force: if True, advertises ALL routes in the forwarding table;
                      otherwise, advertises only those routes that have
                      changed since the last advertisement.
        :return: nothing.
        """
        for dst, fte in self.forwarding_table.items():
            for prt in self.link_latency.keys():
                if self.link_latency[prt] <INFINITY:
                    packet_key = (prt, dst)
                    if not self.POISON_MODE and force:
                        if prt != fte.port:
                            packet = basics.RoutePacket(dst, fte.latency)
                            self.route_hist[packet_key] = packet
                            self.send(packet, prt)
                        # else:
                        #     packet = basics.RoutePacket(fte.port, INFINITY)
                        #     self.route_hist[packet_key] = packet
                        #     self.send(packet, prt)
                    elif self.POISON_MODE and force:
                        if prt != fte.port:
                            packet = basics.RoutePacket(dst, fte.latency)
                            self.route_hist[packet_key] = packet
                            self.send(packet, prt)
                        else:
                            self.send(basics.RoutePacket(dst, INFINITY), prt)
                            self.route_hist[packet_key] = basics.RoutePacket((dst, INFINITY), prt)

                    elif self.POISON_MODE and not force:
                        packet = basics.RoutePacket(dst, fte.latency)
                        if packet_key not in self.route_hist or packet.latency != self.route_hist[packet_key].latency:
                            if prt != fte.port:
                                self.route_hist[packet_key] = packet
                                self.send(packet, prt)
                            else:
                                self.send(basics.RoutePacket(dst, INFINITY),prt)
                                self.route_hist[packet_key] = packet
                    elif not self.POISON_MODE and not force:
                        if prt != fte.port:
                            packet = basics.RoutePacket(dst, fte.latency)
                            if packet_key not in self.route_hist or packet.latency != self.route_hist.get(
                                packet_key).latency or packet.destination!=fte.dst:
                                packet = basics.RoutePacket(dst, fte.latency)
                                self.route_hist[packet_key] = packet
                                self.send(packet, prt)
    def expire_routes(self):
#Clears out expired routes from peer tables; updates forwarding table accordingly.
    for port, peer_table_entry in self.peer_tables.items():
            for host, entry in peer_table_entry.items():
                if entry.expire_time <= api.current_time():
                    if self.POISON_MODE:
                        peer_table_entry[host] = PeerTableEntry(host, INFINITY, api.current_time() + ROUTE_TTL)
                    else:
                        del peer_table_entry[host]
            self.update_forwarding_table()

    def handle_timer(self):
        """
        Called periodically.

        This function simply calls helpers to clear out expired routes and to
        send the forwarding table to neighbors.
        """
        self.expire_routes()
        self.send_routes(force=True)
