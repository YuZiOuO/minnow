Checkpoint 1 Writeup
====================

My name: [your name here]

My SUNet ID: [your sunetid here]

I collaborated with: [list sunetids here]

I would like to thank/reward these classmates for their help: [list sunetids here]

This lab took me about [n] hours to do. I [did/did not] attend the lab session.

I was surprised by or edified to learn that: [describe]

Report from the hands-on component of the lab checkpoint: [include
information from 2.1(4), and report on your experience in 2.2]

Describe Reassembler structure and design. [Describe data structures and
approach taken. Describe alternative designs considered or tested.
Describe benefits and weaknesses of your design compared with
alternatives -- perhaps in terms of simplicity/complexity, risk of
bugs, asymptotic performance, empirical performance, required
implementation time and difficulty, and other factors. Include any
measurements if applicable.]

list<string> was used in my impl to store data cached by the assembler.
The current benefits are that:
1. easy to implement,and low risk of bug.
2. not bad performance,the bottleneck seems to be the O(N) search of the inserting position,
but i thought this would not be a huge effect as the data rougly arrives in sequence.
3. std::set<string> seems to be a better choice as it has logarithmic search time.

Implementation Challenges:
1. deal with partially overlapping data,this brings most of complexity in this impl.

Remaining Bugs:
[]

- Optional: I had unexpected difficulty with: [describe]

- Optional: I think you could make this lab better by: [describe]

- Optional: I'm not sure about: [describe]
