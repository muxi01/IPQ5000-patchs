#!/bin/sh

ip6tables -I INPUT 1 -p tcp --dport 8866 -j ACCEPT
ip6tables -I INPUT 2 -p tcp --dport 9966 -j ACCEPT


