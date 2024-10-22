The compilation should be compatible for C++ versions 11+. <br>
1. Write the process descriptions in input.txt
2. Run ``` g++ myBSSProg_21114057.cpp -o impl -lpthread ``` on the terminal to compile the program.
3. Run ```./impl input.txt ``` to execute the program.

The output on terminal shows all the intermediate processing, once the program has terminated, check output.txt for the final output in the format described in the tutorial.

Sample Input: 
```
begin process p1 
send m1 
end process p1
begin process p2 
recv_B p1 m1 
send m2 
end process p2 
begin process p3 
recv_B p2 m2 
recv_B p1 m1 
end process p3
```

Sample Output:
```
begin process p1
p1 send m1 (1,0,0)
end process p1

begin process p2
recv_B p1 m1 (0,0,0)
recv_A p1 m1 (1,0,0)
p2 send m2 (1,1,0)
end process p2

begin process p3
recv_B p2 m2 (0,0,0)
recv_B p1 m1 (0,0,0)
recv_A p1 m1 (1,0,0)
recv_A p2 m2 (1,1,0)
end process p3
```
Apart from this, input1.txt and input2.txt can be used.

The output1.txt and output2.txt show the outputs generated on these inputs on earlier runs.

Error Detection: Incase of any errors encountered, the output.txt would not be generated and the error would be printed on the terminal