cd ..
make STACK_LSTMCRFMMLabeler
cd example
../STACK_LSTMCRFMMLabeler -l -train ctb/ctb.train.nn.a2b.sample -dev ctb/ctb.dev.nn.a2b.sample -test ctb/ctb.test.nn.a2b.sample -option  option.stack -word giga_cn50.w2v.sample -basemodel basemodel -model model.stack.ctb.pd
