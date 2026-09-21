-- PoChiTaOS Lua sample app
-- Put this file on the SD card and open it from the File Manager.
-- This example demonstrates: string concat, while loop, function, if/else.

function greet(name)
    return "Hello, " .. name .. "!"
end

user = "Pochita"
print(greet(user))

counter = 1
while counter <= 5 do
    print("Loop #" .. counter)
    if counter % 2 == 0 then
        print("Even number")
    else
        print("Odd number")
    end
    counter = counter + 1
    sleep(300)
end

print("Done with loop.")
print("Final message: " .. greet(user))
