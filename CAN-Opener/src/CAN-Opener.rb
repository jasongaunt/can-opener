#!/usr/bin/env ruby -wKU

=begin
	TODO Add application description here 
=end

require 'pp'

# sleep 3

# Open file
File.open("sample.log", "r") do |logline|
	# Set up a counter to step through data at correct rate
	delayTimer = -1.0
	packetList = Hash.new

	# Step through every line
	logline.each do |data|
		# Check to see we have a timestamp to indicate a valid line
		unless data =~ /\[[0-9]+\]/
			next
		end
		# Convert to array
		data = data.strip.split(" ")
		# Separate ID from message
		canID = "0x"+data[1,4].join("").gsub(/^(0)*/, "")
		message = data[5,data.length].join(" ")
		# Check to see if we already have an identical packet in the list
		if packetList.has_key?(canID)
			# One or more messages with this canID have been sent, try and find matching message
			if packetList[canID].has_key?(message)
				# We've seen this message from this canID before, increase the counter
				packetList[canID][message] += 1
			else
				# We've not seen this message from this canID before, start a counter
				packetList[canID][message] = 1
			end
		else
			# We've not seen either this canID nor message before, start tracking it
			packetList[canID] = Hash.new
			packetList[canID][message] = 1
		end
		
		# puts "\e[31m#{canID}\t\e[32m#{message}\e[0m"
		# Sleep for required time to ensure playback in realtime
		data[0] = data[0].gsub(/[\[\]]+/,"").to_f
		if (delayTimer < 0)
			delayTimer = data[0]
		else
			sleep((data[0]-delayTimer)/1000000)
			delayTimer = data[0]
		end
		system("clear")
		packetList.sort.map do |canIDs,messages|
			puts "\e[31m#{canIDs}\e[0m"
			messages.each do |messageData,messageCount|
				puts "\e[34m#{messageCount}\t\e[32m#{messageData}\e[0m"
			end
		end
		sleep(0.25)
	end
end

##
## Common functions
##

# Colourise output
def colorize(text, color_code)
	"#{color_code}#{text}\e[0m"
end
def red(text)
	colorize(text, "\e[31m")
end
def yellow(text)
	colorize(text, "\e[33m")
end
def green(text)
	colorize(text, "\e[32m")
end
def blue(text)
	colorize(text, "\e[34m")
end
def bold(text)
	colorize(text, "\e[0m")
end