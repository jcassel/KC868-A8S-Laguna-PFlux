# KC868-A8S-Laguna-PFlux

Simple firmware to interact with a woodshop dust collector system. 
Each blast gate has a switch on it that when opened it will send an MQTT message requesting that something be turned on. 
In this case the request is then picked up by an RF Gateway(sonoff) running Tosmota firmware. That in turn sends out an RF singnal to turn on or off a Lagauna P-Flux 
Vaccume Dust collector equipped with an RF reciever from the factory. 

It is also a good simple example of how to use the IO on a Kincony KC868-A8S Controller board.



