from mininet.topo import Topo
from mininet.cli import CLI
from mininet.term import tunnelX11
import os
import signal
import time


# Usage example:
# sudo mn --mac --custom he1-mn-topo.py --topo he1 --link tc

terms = []

class H1Topo( Topo ):
    "Larger topology for Home Exam 1."

    def __init__( self ):
        "Set up our custom topo."

        # Initialize topology
        Topo.__init__( self )

        # Add hosts
        A = self.addHost('A')
        B = self.addHost('B')
        C = self.addHost('C')
        D = self.addHost('D')
        E = self.addHost('E')

        # Add links
        self.addLink(A, B, bw=10, delay='10ms', loss=0.0, use_tbf=False)
        self.addLink(B, C, bw=10, delay='10ms', loss=0.0, use_tbf=False)
        self.addLink(B, D, bw=10, delay='10ms', loss=0.0, use_tbf=False)
        self.addLink(C, D, bw=10, delay='10ms', loss=0.0, use_tbf=False)
        self.addLink(D, E, bw=10, delay='10ms', loss=0.0, use_tbf=False)


def openTerm(self, node, title, geometry, cmd="bash"):
    "Open xterm window."

    display, tunnel = tunnelX11(node)

    return node.popen(["xterm",
                       "-hold",
                       "-title", "'%s'" % title,
                       "-geometry", geometry,
                       "-display", display,
                       "-e", cmd])

def init_exam(self, line):
    "init is an example command to extend the Mininet CLI"

    net = self.mn
    A = net.get('A')
    B = net.get('B')
    C = net.get('C')
    D = net.get('D')
    E = net.get('E')

    # Start MIP daemons
    terms.append(openTerm(self,
                          node=A,
                          title="Host A",
                          geometry="80x20+0+150",
                          cmd="echo 'Node A' && bash"))
    
    terms.append(openTerm(self,
                          node=A,
                          title="Host A",
                          geometry="80x20+0+450",
                          cmd="echo 'Node A' && bash"))

    terms.append(openTerm(self,
                          node=B,
                          title="Host A",
                          geometry="80x20+500+150",
                          cmd="echo 'Node B' && bash"))
    
    terms.append(openTerm(self,
                          node=B,
                          title="Host A",
                          geometry="80x20+500+450",
                          cmd="echo 'Node B' && bash"))


CLI.do_init_exam = init_exam

# Inside mininet console run 'EOF' to gracefully kill the mininet console
orig_EOF = CLI.do_EOF

topos = { 'he1': ( lambda: H1Topo() ) }

