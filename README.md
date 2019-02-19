# Project Maisie

## A sensor chain

# Aim

Collect senors' data then save them in database to review later.

# Creator

Muntadhar Haydar

# Story

I created this project as mini-project during my senior year of electrical engineering degree trying to build a sensor-array that's felxible enough for an n nodes, simple to install and maintaine and also keeping an eye on the total cost.

# Techincallity

The premise is simple, you have your first (n=zero) node that's capable of connecting to the internet and connects to other nodes via RF 2.4GHz frequency.

A command will be
  
 targetNodeId-sourceNodeId-CMD

The "targetNodeId" must equal the Id of the receiving Node to continue processing data.

The "sourceNodeId" must be

    nodeId-1 or nodeId+1

i.e. either the next or previous node.

Obviously though, CMD is the command string

# How To Use

Upload the `PrimaryNode.ino` to ONE node and `SecondaryNode.ino` to whatever number **BUT** don't forget to increase the node's Id
